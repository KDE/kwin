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
#if HAVE_GBM
#include "drm_buffer_gbm.h"
#endif

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

DrmOutput::DrmOutput(DrmGpu *gpu, DrmPipeline *pipeline)
    : DrmAbstractOutput(gpu)
    , m_pipeline(pipeline)
    , m_connector(pipeline->connector())
{
    m_pipeline->setOutput(this);
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
    if (conn->hasRgbRange()) {
        setCapabilityInternal(Capability::RgbRange);
        setRgbRangeInternal(conn->rgbRange());
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
    m_pipeline->setOutput(nullptr);
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
    const Cursor * const cursor = Cursors::self()->currentCursor();
    if (m_pipeline->setCursor(m_cursor, logicalToNativeMatrix(cursor->rect(), scale(), transform()).map(cursor->hotspot()) )) {
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
    if (m_pipeline->setCursor(m_cursor, logicalToNativeMatrix(cursor->rect(), scale(), transform()).map(cursor->hotspot()) )) {
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

QVector<AbstractWaylandOutput::Mode> DrmOutput::getModes() const
{
    bool modeFound = false;
    QVector<Mode> modes;
    auto conn = m_pipeline->connector();
    auto modelist = conn->modes();

    modes.reserve(modelist.count());
    for (int i = 0; i < modelist.count(); ++i) {
        Mode mode;
        if (i == conn->currentModeIndex()) {
            mode.flags |= ModeFlag::Current;
            modeFound = true;
        }
        if (modelist[i].mode.type & DRM_MODE_TYPE_PREFERRED) {
            mode.flags |= ModeFlag::Preferred;
        }

        mode.id = i;
        mode.size = modelist[i].size;
        mode.refreshRate = modelist[i].refreshRate;
        modes << mode;
    }
    if (!modeFound) {
        // select first mode by default
        modes[0].flags |= ModeFlag::Current;
    }
    return modes;
}

void DrmOutput::initOutputDevice()
{
    const auto conn = m_pipeline->connector();
    setName(conn->connectorName());
    initialize(conn->modelName(), conn->edid()->manufacturerString(),
               conn->edid()->eisaId(), conn->edid()->serialNumber(),
               conn->physicalSize(), getModes(), conn->edid()->raw());
}

void DrmOutput::updateEnablement(bool enable)
{
    if (m_pipeline->setActive(enable)) {
        m_gpu->platform()->enableOutput(this, enable);
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
            m_gpu->platform()->createDpmsFilter();
        }
    } else {
        m_turnOffTimer.stop();
        setDrmDpmsMode(mode);

        if (mode != dpmsMode()) {
            setDpmsModeInternal(mode);
            Q_EMIT wakeUp();
        }
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
            m_gpu->platform()->checkOutputsAreOn();
            if (Compositor *compositor = Compositor::self()) {
                compositor->addRepaintFull();
            }
        } else {
            m_renderLoop->inhibit();
            m_gpu->platform()->createDpmsFilter();
        }
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

void DrmOutput::updateTransform(Transform transform)
{
    setTransformInternal(transform);
    const auto planeTransform = outputToPlaneTransform(transform);
    if (!qEnvironmentVariableIsSet("KWIN_DRM_SW_ROTATIONS_ONLY")
        && !m_pipeline->setTransformation(planeTransform)) {
        qCDebug(KWIN_DRM) << "setting transformation to" << planeTransform << "failed!";
        // just in case, if we had any rotation before, clear it
        m_pipeline->setTransformation(DrmPlane::Transformation::Rotate0);
    }

    // show cursor only if is enabled, i.e if pointer device is present
    if (!m_gpu->platform()->isCursorHidden() && !m_gpu->platform()->usesSoftwareCursor()) {
        // the cursor might need to get rotated
        showCursor();
        updateCursor();
    }
}

void DrmOutput::updateModes()
{
    auto conn = m_pipeline->connector();
    conn->updateModes();

    const auto modes = getModes();
    setModes(modes);

    auto it = std::find_if(modes.constBegin(), modes.constEnd(),
        [](const AbstractWaylandOutput::Mode &mode){
            return mode.flags.testFlag(ModeFlag::Current);
        }
    );
    Q_ASSERT(it != modes.constEnd());
    AbstractWaylandOutput::Mode mode = *it;

    // mode changed
    if (mode.size != modeSize() || mode.refreshRate != refreshRate()) {
        applyMode(mode.id);
    }
}

void DrmOutput::updateMode(const QSize &size, uint32_t refreshRate)
{
    auto conn = m_pipeline->connector();
    if (conn->currentMode().size == size && conn->currentMode().refreshRate == refreshRate) {
        return;
    }
    // try to find a fitting mode
    auto modelist = conn->modes();
    for (int i = 0; i < modelist.size(); i++) {
        if (modelist[i].size == size && modelist[i].refreshRate == refreshRate) {
            applyMode(i);
            return;
        }
    }
    qCWarning(KWIN_DRM, "Could not find a fitting mode with size=%dx%d and refresh rate %d for output %s",
              size.width(), size.height(), refreshRate, qPrintable(name()));
}

void DrmOutput::applyMode(int modeIndex)
{
    if (m_pipeline->modeset(modeIndex)) {
        auto mode = m_pipeline->connector()->currentMode();
        AbstractWaylandOutput::setCurrentModeInternal(mode.size, mode.refreshRate);
        m_renderLoop->setRefreshRate(mode.refreshRate);
    }
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

DrmConnector *DrmOutput::connector() const
{
    return m_connector;
}

DrmPipeline *DrmOutput::pipeline() const
{
    return m_pipeline;
}

GbmBuffer *DrmOutput::currentBuffer() const
{
#if HAVE_GBM
    return dynamic_cast<GbmBuffer*>(m_pipeline->currentBuffer());
#else
    return nullptr;
#endif
}

bool DrmOutput::isDpmsEnabled() const
{
    return m_pipeline->isActive();
}

QSize DrmOutput::sourceSize() const
{
    return m_pipeline->sourceSize();
}

bool DrmOutput::isFormatSupported(uint32_t drmFormat) const
{
    return m_pipeline->isFormatSupported(drmFormat);
}

QVector<uint64_t> DrmOutput::supportedModifiers(uint32_t drmFormat) const
{
    return m_pipeline->supportedModifiers(drmFormat);
}

void DrmOutput::setRgbRange(RgbRange range)
{
    if (m_pipeline->setRgbRange(range)) {
        setRgbRangeInternal(range);
        m_renderLoop->scheduleRepaint();
    }
}

void DrmOutput::setPipeline(DrmPipeline *pipeline)
{
    Q_ASSERT_X(!pipeline || pipeline->connector() == m_connector, "DrmOutput::setPipeline", "Pipeline with wrong connector set!");
    m_pipeline = pipeline;
}

}
