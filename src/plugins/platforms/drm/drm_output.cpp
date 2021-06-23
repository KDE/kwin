/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_output.h"
#include "drm_backend.h"
#include "drm_object_crtc.h"
#include "drm_object_connector.h"
#include "drm_gpu.h"
#include "drm_pipeline.h"

#include "composite.h"
#include "cursor.h"
#include "logging.h"
#include "main.h"
#include "renderloop.h"
#include "renderloop_p.h"
#include "screens.h"
#include "session.h"
// Qt
#include <QMatrix4x4>
#include <QCryptographicHash>
#include <QPainter>
// c++
#include <cerrno>
// drm
#include <xf86drm.h>
#include <libdrm/drm_mode.h>

namespace KWin
{

DrmOutput::DrmOutput(DrmBackend *backend, DrmGpu *gpu, DrmPipeline *pipeline)
    : AbstractWaylandOutput(backend)
    , m_backend(backend)
    , m_gpu(gpu)
    , m_pipeline(pipeline)
    , m_renderLoop(new RenderLoop(this))
{
    m_pipeline->setUserData(this);
    auto conn = m_pipeline->connector();
    m_renderLoop->setRefreshRate(conn->currentMode().refreshRate);
    setSubPixelInternal(conn->subpixel());
    setInternal(conn->isInternal());
    setCapabilityInternal(DrmOutput::Capability::Dpms);
    if (conn->hasOverscan()) {
        setCapabilityInternal(Capability::Overscan);
        setOverscanInternal(conn->overscan());
    }
    if (conn->vrrCapable()) {
        setCapabilityInternal(Capability::Vrr);
        setVrrPolicy(RenderLoop::VrrPolicy::Automatic);
    }
    initOutputDevice();

    m_turnOffTimer.setSingleShot(true);
    m_turnOffTimer.setInterval(dimAnimationTime());
    connect(&m_turnOffTimer, &QTimer::timeout, this, [this] {
        setDrmDpmsMode(DpmsMode::Off);
    });
}

DrmOutput::~DrmOutput()
{
    hideCursor();
    if (m_pageFlipPending) {
        pageFlipped();
    }
}

RenderLoop *DrmOutput::renderLoop() const
{
    return m_renderLoop;
}

bool DrmOutput::initCursor(const QSize &cursorSize)
{
    m_cursor = QSharedPointer<DrmDumbBuffer>::create(m_gpu, cursorSize);
    if (!m_cursor->map(QImage::Format_ARGB32_Premultiplied)) {
        return false;
    }
    return updateCursor();
}

bool DrmOutput::hideCursor()
{
    bool visibleBefore = m_pipeline->isCursorVisible();
    if (m_pipeline->setCursor(nullptr)) {
        if (RenderLoopPrivate::get(m_renderLoop)->presentMode == RenderLoopPrivate::SyncMode::Adaptive
            && visibleBefore) {
            m_renderLoop->scheduleRepaint();
        }
        return true;
    } else {
        return false;
    }
}

bool DrmOutput::showCursor()
{
    bool visibleBefore = m_pipeline->isCursorVisible();
    if (m_pipeline->setCursor(m_cursor)) {
        if (RenderLoopPrivate::get(m_renderLoop)->presentMode == RenderLoopPrivate::SyncMode::Adaptive
            && !visibleBefore
            && m_pipeline->isCursorVisible()) {
            m_renderLoop->scheduleRepaint();
        }
        return true;
    } else {
        return false;
    }
}

static bool isCursorSpriteCompatible(const QImage *buffer, const QImage *sprite)
{
    // Note that we need compare the rects in the device independent pixels because the
    // buffer and the cursor sprite image may have different scale factors.
    const QRect bufferRect(QPoint(0, 0), buffer->size() / buffer->devicePixelRatio());
    const QRect spriteRect(QPoint(0, 0), sprite->size() / sprite->devicePixelRatio());

    return bufferRect.contains(spriteRect);
}

bool DrmOutput::updateCursor()
{
    const Cursor *cursor = Cursors::self()->currentCursor();
    if (!cursor) {
        hideCursor();
        return true;
    }
    const QImage cursorImage = cursor->image();
    if (cursorImage.isNull()) {
        hideCursor();
        return true;
    }
    QImage *c = m_cursor->image();
    c->setDevicePixelRatio(scale());
    if (!isCursorSpriteCompatible(c, &cursorImage)) {
        // If the cursor image is too big, fall back to rendering the software cursor.
        return false;
    }
    c->fill(Qt::transparent);
    QPainter p;
    p.begin(c);
    p.setWorldTransform(logicalToNativeMatrix(cursor->rect(), 1, transform()).toTransform());
    p.drawImage(QPoint(0, 0), cursorImage);
    p.end();
    if (m_pipeline->setCursor(m_cursor)) {
        if (RenderLoopPrivate::get(m_renderLoop)->presentMode == RenderLoopPrivate::SyncMode::Adaptive
            && m_pipeline->isCursorVisible()) {
            m_renderLoop->scheduleRepaint();
        }
        return true;
    } else {
        return false;
    }
}

bool DrmOutput::moveCursor()
{
    Cursor *cursor = Cursors::self()->currentCursor();
    const QMatrix4x4 hotspotMatrix = logicalToNativeMatrix(cursor->rect(), scale(), transform());
    const QMatrix4x4 monitorMatrix = logicalToNativeMatrix(geometry(), scale(), transform());

    QPoint pos = monitorMatrix.map(cursor->pos());
    pos -= hotspotMatrix.map(cursor->hotspot());

    bool visibleBefore = m_pipeline->isCursorVisible();
    if (pos != m_pipeline->cursorPos()) {
        if (!m_pipeline->moveCursor(pos)) {
            return false;
        }
        if (RenderLoopPrivate::get(m_renderLoop)->presentMode == RenderLoopPrivate::SyncMode::Adaptive
            && (visibleBefore || m_pipeline->isCursorVisible())) {
            m_renderLoop->scheduleRepaint();
        }
    }
    return true;
}

void DrmOutput::initOutputDevice()
{
    auto conn = m_pipeline->connector();
    auto modelist = conn->modes();

    QVector<Mode> modes;
    modes.reserve(modelist.count());
    for (int i = 0; i < modelist.count(); ++i) {
        Mode mode;
        if (i == conn->currentModeIndex()) {
            mode.flags |= ModeFlag::Current;
        }
        if (modelist[i].mode.type & DRM_MODE_TYPE_PREFERRED) {
            mode.flags |= ModeFlag::Preferred;
        }

        mode.id = i;
        mode.size = modelist[i].size;
        mode.refreshRate = modelist[i].refreshRate;
        modes << mode;
    }

    setName(conn->connectorName());
    initialize(conn->modelName(), conn->edid()->manufacturerString(),
               conn->edid()->eisaId(), conn->edid()->serialNumber(),
               conn->physicalSize(), modes, conn->edid()->raw());
}

void DrmOutput::updateEnablement(bool enable)
{
    if (m_pipeline->setActive(enable)) {
        m_backend->enableOutput(this, enable);
    } else {
        qCCritical(KWIN_DRM) << "failed to update enablement to" << enable;
    }
}

void DrmOutput::setDpmsMode(DpmsMode mode)
{
    if (mode == DpmsMode::Off) {
        if (!m_turnOffTimer.isActive()) {
            Q_EMIT aboutToTurnOff(std::chrono::milliseconds(m_turnOffTimer.interval()));
            m_turnOffTimer.start();
        }
        if (isEnabled()) {
            m_backend->createDpmsFilter();
        }
    } else {
        m_turnOffTimer.stop();
        setDrmDpmsMode(mode);
        Q_EMIT wakeUp();
    }
}

void DrmOutput::setDrmDpmsMode(DpmsMode mode)
{
    if (!isEnabled()) {
        return;
    }
    bool active = mode == DpmsMode::On;
    if (active == m_pipeline->isActive()) {
        setDpmsModeInternal(mode);
        return;
    }
    if (m_pipeline->setActive(active)) {
        setDpmsModeInternal(mode);
        if (active) {
            m_renderLoop->uninhibit();
            m_backend->checkOutputsAreOn();
            if (Compositor *compositor = Compositor::self()) {
                compositor->addRepaintFull();
            }
        } else {
            m_renderLoop->inhibit();
            m_backend->createDpmsFilter();
        }
    } else {
        qCCritical(KWIN_DRM) << "failed to set active to" << active;
    }
}

DrmPlane::Transformations outputToPlaneTransform(DrmOutput::Transform transform)
{
    using OutTrans = DrmOutput::Transform;
    using PlaneTrans = DrmPlane::Transformation;

    // TODO: Do we want to support reflections (flips)?

    switch (transform) {
    case OutTrans::Normal:
    case OutTrans::Flipped:
        return PlaneTrans::Rotate0;
    case OutTrans::Rotated90:
    case OutTrans::Flipped90:
        return PlaneTrans::Rotate90;
    case OutTrans::Rotated180:
    case OutTrans::Flipped180:
        return PlaneTrans::Rotate180;
    case OutTrans::Rotated270:
    case OutTrans::Flipped270:
        return PlaneTrans::Rotate270;
    default:
        Q_UNREACHABLE();
    }
}

bool DrmOutput::hardwareTransforms() const
{
    return m_pipeline->transformation() == outputToPlaneTransform(transform());
}

void DrmOutput::updateTransform(Transform transform)
{
    const auto planeTransform = outputToPlaneTransform(transform);
    if (!qEnvironmentVariableIsSet("KWIN_DRM_SW_ROTATIONS_ONLY")
        && !m_pipeline->setTransformation(planeTransform)) {
        qCDebug(KWIN_DRM) << "setting transformation to" << planeTransform << "failed!";
        // just in case, if we had any rotation before, clear it
        m_pipeline->setTransformation(DrmPlane::Transformation::Rotate0);
    }

    // show cursor only if is enabled, i.e if pointer device is present
    if (!m_backend->isCursorHidden() && !m_backend->usesSoftwareCursor()) {
        // the cursor might need to get rotated
        showCursor();
        updateCursor();
    }
}

void DrmOutput::updateMode(uint32_t width, uint32_t height, uint32_t refreshRate)
{
    auto conn = m_pipeline->connector();
    if (conn->currentMode().size == QSize(width, height) && conn->currentMode().refreshRate == refreshRate) {
        return;
    }
    auto modelist = conn->modes();
    for (int i = 0; i < modelist.size(); i++) {
        if (modelist[i].size == QSize(width, height) && modelist[i].refreshRate == refreshRate) {
            updateMode(i);
            return;
        }
    }
    qCWarning(KWIN_DRM, "Could not find a fitting mode with size=%dx%d and refresh rate %d for output %s",
              width, height, refreshRate, qPrintable(name()));
}

void DrmOutput::updateMode(int modeIndex)
{
    m_pipeline->modeset(modeIndex);
    auto mode = m_pipeline->connector()->currentMode();
    AbstractWaylandOutput::setCurrentModeInternal(mode.size, mode.refreshRate);
    m_renderLoop->setRefreshRate(mode.refreshRate);
}

void DrmOutput::pageFlipped()
{
    Q_ASSERT(m_pageFlipPending || !m_gpu->atomicModeSetting());
    m_pageFlipPending = false;
    m_pipeline->pageFlipped();
}

bool DrmOutput::present(const QSharedPointer<DrmBuffer> &buffer, QRegion damagedRegion)
{
    if (!buffer || buffer->bufferId() == 0) {
        return false;
    }
    RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_renderLoop);
    if (!m_pipeline->setSyncMode(renderLoopPrivate->presentMode)) {
        setVrrPolicy(RenderLoop::VrrPolicy::Never);
    }
    if (m_pipeline->present(buffer)) {
        m_pageFlipPending = true;
        Q_EMIT outputChange(damagedRegion);
        return true;
    } else {
        return false;
    }
}

int DrmOutput::gammaRampSize() const
{
    return m_pipeline->crtc()->gammaRampSize();
}

bool DrmOutput::setGammaRamp(const GammaRamp &gamma)
{
    if (m_pipeline->setGammaRamp(gamma)) {
        m_renderLoop->scheduleRepaint();
        return true;
    } else {
        return false;
    }
}

void DrmOutput::setOverscan(uint32_t overscan)
{
    if (m_pipeline->setOverscan(overscan)) {
        setOverscanInternal(overscan);
        m_renderLoop->scheduleRepaint();
    }
}

DrmPipeline *DrmOutput::pipeline() const
{
    return m_pipeline;
}

DrmBuffer *DrmOutput::currentBuffer() const
{
    return m_pipeline->currentBuffer();
}

bool DrmOutput::isDpmsEnabled() const
{
    return m_pipeline->isActive();
}

QSize DrmOutput::sourceSize() const
{
    if (m_pipeline) {
        return m_pipeline->sourceSize();
    } else {
        return modeSize();
    }
}

void DrmOutput::setPipeline(DrmPipeline *pipeline)
{
    m_pipeline = pipeline;
}

}
