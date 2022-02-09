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
#include "drm_buffer.h"

#include "composite.h"
#include "cursor.h"
#include "logging.h"
#include "main.h"
#include "renderloop.h"
#include "renderloop_p.h"
#include "scene.h"
#include "screens.h"
#include "session.h"
#include "waylandoutputconfig.h"
#include "dumb_swapchain.h"
#include "cursor.h"
// Qt
#include <QMatrix4x4>
#include <QCryptographicHash>
#include <QPainter>
// c++
#include <cerrno>
// drm
#include <xf86drm.h>
#include <libdrm/drm_mode.h>
#include <drm_fourcc.h>

namespace KWin
{

DrmOutput::DrmOutput(DrmPipeline *pipeline)
    : DrmAbstractOutput(pipeline->connector()->gpu())
    , m_pipeline(pipeline)
    , m_connector(pipeline->connector())
{
    m_pipeline->setOutput(this);
    auto conn = m_pipeline->connector();
    m_renderLoop->setRefreshRate(conn->currentMode()->refreshRate());
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

    connect(Cursors::self(), &Cursors::currentCursorChanged, this, &DrmOutput::updateCursor);
    connect(Cursors::self(), &Cursors::hiddenChanged, this, &DrmOutput::updateCursor);
    connect(Cursors::self(), &Cursors::positionChanged, this, &DrmOutput::moveCursor);
}

DrmOutput::~DrmOutput()
{
    m_pipeline->setOutput(nullptr);
}

static bool isCursorSpriteCompatible(const QImage *buffer, const QImage *sprite)
{
    // Note that we need compare the rects in the device independent pixels because the
    // buffer and the cursor sprite image may have different scale factors.
    const QRect bufferRect(QPoint(0, 0), buffer->size() / buffer->devicePixelRatio());
    const QRect spriteRect(QPoint(0, 0), sprite->size() / sprite->devicePixelRatio());

    return bufferRect.contains(spriteRect);
}

void DrmOutput::updateCursor()
{
    static bool valid;
    static const bool forceSoftwareCursor = qEnvironmentVariableIntValue("KWIN_FORCE_SW_CURSOR", &valid) == 1 && valid;
    if (forceSoftwareCursor) {
        m_setCursorSuccessful = false;
        return;
    }
    if (!m_pipeline->pending.crtc) {
        return;
    }
    const Cursor *cursor = Cursors::self()->currentCursor();
    if (!cursor) {
        m_pipeline->setCursor(nullptr);
        return;
    }
    const QImage cursorImage = cursor->image();
    if (cursorImage.isNull() || Cursors::self()->isCursorHidden()) {
        m_pipeline->setCursor(nullptr);
        return;
    }
    if (m_cursor && m_cursor->isEmpty()) {
        m_pipeline->setCursor(nullptr);
        return;
    }
    const auto plane = m_pipeline->pending.crtc->cursorPlane();
    if (!m_cursor || (plane && !plane->formats().value(m_cursor->drmFormat()).contains(DRM_FORMAT_MOD_LINEAR))) {
        if (plane) {
            const auto formatModifiers = plane->formats();
            const auto formats = formatModifiers.keys();
            for (uint32_t format : formats) {
                if (!formatModifiers[format].contains(DRM_FORMAT_MOD_LINEAR)) {
                    continue;
                }
                m_cursor = QSharedPointer<DumbSwapchain>::create(m_gpu, m_gpu->cursorSize(), format, QImage::Format::Format_ARGB32_Premultiplied);
                if (!m_cursor->isEmpty()) {
                    break;
                }
            }
        } else {
            m_cursor = QSharedPointer<DumbSwapchain>::create(m_gpu, m_gpu->cursorSize(), DRM_FORMAT_XRGB8888, QImage::Format::Format_ARGB32_Premultiplied);
        }
        if (!m_cursor || m_cursor->isEmpty()) {
            m_pipeline->setCursor(nullptr);
            m_setCursorSuccessful = false;
            return;
        }
    }
    m_cursor->releaseBuffer(m_cursor->currentBuffer());
    m_cursor->acquireBuffer();
    QImage *c = m_cursor->currentBuffer()->image();
    c->setDevicePixelRatio(scale());
    if (!isCursorSpriteCompatible(c, &cursorImage)) {
        // If the cursor image is too big, fall back to rendering the software cursor.
        m_pipeline->setCursor(nullptr);
        m_setCursorSuccessful = false;
        return;
    }
    c->fill(Qt::transparent);
    QPainter p;
    p.begin(c);
    p.setWorldTransform(logicalToNativeMatrix(cursor->rect(), 1, transform()).toTransform());
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(QPoint(0, 0), cursorImage);
    p.end();
    m_setCursorSuccessful = m_pipeline->setCursor(m_cursor->currentBuffer(), logicalToNativeMatrix(cursor->rect(), scale(), transform()).map(cursor->hotspot()));
    moveCursor();
}

void DrmOutput::moveCursor()
{
    if (!m_setCursorSuccessful || !m_pipeline->pending.crtc) {
        return;
    }
    Cursor *cursor = Cursors::self()->currentCursor();
    const QMatrix4x4 monitorMatrix = logicalToNativeMatrix(geometry(), scale(), transform());
    const QMatrix4x4 hotspotMatrix = logicalToNativeMatrix(cursor->rect(), scale(), transform());
    m_moveCursorSuccessful = m_pipeline->moveCursor(monitorMatrix.map(cursor->pos()) - hotspotMatrix.map(cursor->hotspot()));
    if (!m_moveCursorSuccessful) {
        m_pipeline->setCursor(nullptr);
    }
}

QVector<AbstractWaylandOutput::Mode> DrmOutput::getModes() const
{
    bool modeFound = false;
    QVector<Mode> modes;
    auto conn = m_pipeline->connector();
    QVector<DrmConnectorMode *> modelist = conn->modes();

    modes.reserve(modelist.count());
    for (int i = 0; i < modelist.count(); ++i) {
        Mode mode;
        if (i == conn->currentModeIndex()) {
            mode.flags |= ModeFlag::Current;
            modeFound = true;
        }
        if (modelist[i]->nativeMode()->type & DRM_MODE_TYPE_PREFERRED) {
            mode.flags |= ModeFlag::Preferred;
        }

        mode.id = i;
        mode.size = modelist[i]->size();
        mode.refreshRate = modelist[i]->refreshRate();
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
    m_gpu->platform()->enableOutput(this, enable);
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
        if (mode != dpmsMode() && setDrmDpmsMode(mode)) {
            Q_EMIT wakeUp();
        }
    }
}

bool DrmOutput::setDrmDpmsMode(DpmsMode mode)
{
    if (!isEnabled()) {
        return false;
    }
    bool active = mode == DpmsMode::On;
    bool isActive = dpmsMode() == DpmsMode::On;
    if (active == isActive) {
        setDpmsModeInternal(mode);
        return true;
    }
    m_pipeline->pending.active = active;
    if (DrmPipeline::commitPipelines({m_pipeline}, active ? DrmPipeline::CommitMode::Test : DrmPipeline::CommitMode::CommitModeset)) {
        m_pipeline->applyPendingChanges();
        setDpmsModeInternal(mode);
        if (active) {
            m_renderLoop->uninhibit();
            m_gpu->platform()->checkOutputsAreOn();
            if (Compositor::compositing()) {
                Compositor::self()->scene()->addRepaintFull();
            }
        } else {
            m_renderLoop->inhibit();
            m_gpu->platform()->createDpmsFilter();
        }
        return true;
    } else {
        qCWarning(KWIN_DRM) << "Setting dpms mode failed!";
        m_pipeline->revertPendingChanges();
        if (isEnabled() && isActive && !active) {
            m_gpu->platform()->checkOutputsAreOn();
        }
        return false;
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
        m_pipeline->pending.modeIndex = mode.id;
        if (DrmPipeline::commitPipelines({m_pipeline}, DrmPipeline::CommitMode::Test)) {
            m_pipeline->applyPendingChanges();
            auto mode = m_pipeline->connector()->currentMode();
            setCurrentModeInternal(mode->size(), mode->refreshRate());
            m_renderLoop->setRefreshRate(mode->refreshRate());
        } else {
            qCWarning(KWIN_DRM) << "Setting changed mode failed!";
            m_pipeline->revertPendingChanges();
        }
    }
}

bool DrmOutput::needsSoftwareTransformation() const
{
    return m_pipeline->pending.bufferTransformation != m_pipeline->pending.sourceTransformation;
}

bool DrmOutput::present(const QSharedPointer<DrmBuffer> &buffer, QRegion damagedRegion)
{
    if (!buffer || buffer->bufferId() == 0) {
        presentFailed();
        return false;
    }
    RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_renderLoop);
    if (m_pipeline->pending.syncMode != renderLoopPrivate->presentMode) {
        m_pipeline->pending.syncMode = renderLoopPrivate->presentMode;
        if (DrmPipeline::commitPipelines({m_pipeline}, DrmPipeline::CommitMode::Test)) {
            m_pipeline->applyPendingChanges();
        } else {
            m_pipeline->revertPendingChanges();
            setVrrPolicy(RenderLoop::VrrPolicy::Never);
        }
    }
    if (m_pipeline->present(buffer)) {
        Q_EMIT outputChange(damagedRegion);
        return true;
    } else {
        return false;
    }
}

int DrmOutput::gammaRampSize() const
{
    return m_pipeline->pending.crtc ? m_pipeline->pending.crtc->gammaRampSize() : 256;
}

bool DrmOutput::setGammaRamp(const GammaRamp &gamma)
{
    m_pipeline->pending.gamma = QSharedPointer<DrmGammaRamp>::create(m_gpu, gamma);
    if (DrmPipeline::commitPipelines({m_pipeline}, DrmPipeline::CommitMode::Test)) {
        m_pipeline->applyPendingChanges();
        m_renderLoop->scheduleRepaint();
        return true;
    } else {
        m_pipeline->revertPendingChanges();
        return false;
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

QSize DrmOutput::bufferSize() const
{
    return m_pipeline->bufferSize();
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

bool DrmOutput::queueChanges(const WaylandOutputConfig &config)
{
    static bool valid;
    static int envOnlySoftwareRotations = qEnvironmentVariableIntValue("KWIN_DRM_SW_ROTATIONS_ONLY", &valid) == 1 || !valid;

    auto props = config.constChangeSet(this);
    m_pipeline->pending.active = props->enabled;
    auto modelist = m_connector->modes();
    int index = -1;
    for (int i = 0; i < modelist.size(); i++) {
        if (modelist[i]->size() == props->modeSize && modelist[i]->refreshRate() == props->refreshRate) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        qCWarning(KWIN_DRM).nospace() << "Could not find mode " << props->modeSize << "@" << props->refreshRate << " for output " << this;
        return false;
    }
    m_pipeline->pending.modeIndex = index;
    m_pipeline->pending.overscan = props->overscan;
    m_pipeline->pending.rgbRange = props->rgbRange;
    m_pipeline->pending.sourceTransformation = outputToPlaneTransform(props->transform);
    if (!envOnlySoftwareRotations && m_gpu->atomicModeSetting()) {
        m_pipeline->pending.bufferTransformation = m_pipeline->pending.sourceTransformation;
    }
    m_pipeline->pending.enabled = props->enabled;
    return true;
}

void DrmOutput::applyQueuedChanges(const WaylandOutputConfig &config)
{
    if (!m_connector->isConnected()) {
        return;
    }
    Q_EMIT aboutToChange();
    m_pipeline->applyPendingChanges();

    auto props = config.constChangeSet(this);
    setEnabled(props->enabled && m_pipeline->pending.crtc);
    moveTo(props->pos);
    setScale(props->scale);
    setTransformInternal(props->transform);

    m_connector->setModeIndex(m_pipeline->pending.modeIndex);
    auto mode = m_connector->currentMode();
    setCurrentModeInternal(mode->size(), mode->refreshRate());
    m_renderLoop->setRefreshRate(mode->refreshRate());
    setOverscanInternal(m_pipeline->pending.overscan);
    setRgbRangeInternal(m_pipeline->pending.rgbRange);
    setVrrPolicy(props->vrrPolicy);

    m_renderLoop->scheduleRepaint();
    Q_EMIT changed();
}

void DrmOutput::revertQueuedChanges()
{
    m_pipeline->revertPendingChanges();
}

void DrmOutput::pageFlipped(std::chrono::nanoseconds timestamp)
{
    RenderLoopPrivate::get(m_renderLoop)->notifyFrameCompleted(timestamp);
}

void DrmOutput::presentFailed()
{
    RenderLoopPrivate::get(m_renderLoop)->notifyFrameFailed();
}

int DrmOutput::maxBpc() const
{
    auto prop = m_connector->getProp(DrmConnector::PropertyIndex::MaxBpc);
    return prop ? prop->maxValue() : 8;
}

bool DrmOutput::usesSoftwareCursor() const
{
    return !m_setCursorSuccessful || !m_moveCursorSuccessful;
}

}
