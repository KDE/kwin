/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_output.h"
#include "drm_backend.h"
#include "drm_buffer.h"
#include "drm_gpu.h"
#include "drm_object_connector.h"
#include "drm_object_crtc.h"
#include "drm_pipeline.h"

#include "composite.h"
#include "cursor.h"
#include "drm_layer.h"
#include "dumb_swapchain.h"
#include "logging.h"
#include "main.h"
#include "outputconfiguration.h"
#include "renderloop.h"
#include "renderloop_p.h"
#include "scene.h"
#include "screens.h"
#include "session.h"
// Qt
#include <QCryptographicHash>
#include <QMatrix4x4>
#include <QPainter>
// c++
#include <cerrno>
// drm
#include <drm_fourcc.h>
#include <libdrm/drm_mode.h>
#include <xf86drm.h>

namespace KWin
{

DrmOutput::DrmOutput(DrmPipeline *pipeline)
    : DrmAbstractOutput(pipeline->connector()->gpu())
    , m_pipeline(pipeline)
    , m_connector(pipeline->connector())
{
    m_pipeline->setOutput(this);
    const auto conn = m_pipeline->connector();
    m_renderLoop->setRefreshRate(m_pipeline->pending.mode->refreshRate());

    Capabilities capabilities = Capability::Dpms;
    if (conn->hasOverscan()) {
        capabilities |= Capability::Overscan;
        setOverscanInternal(conn->overscan());
    }
    if (conn->vrrCapable()) {
        capabilities |= Capability::Vrr;
        setVrrPolicy(RenderLoop::VrrPolicy::Automatic);
    }
    if (conn->hasRgbRange()) {
        capabilities |= Capability::RgbRange;
        setRgbRangeInternal(conn->rgbRange());
    }

    const Edid *edid = conn->edid();

    setInformation(Information{
        .name = conn->connectorName(),
        .manufacturer = edid->manufacturerString(),
        .model = conn->modelName(),
        .serialNumber = edid->serialNumber(),
        .eisaId = edid->eisaId(),
        .physicalSize = conn->physicalSize(),
        .edid = edid->raw(),
        .subPixel = conn->subpixel(),
        .capabilities = capabilities,
        .internal = conn->isInternal(),
    });

    const QList<QSharedPointer<OutputMode>> modes = getModes();
    QSharedPointer<OutputMode> currentMode = m_pipeline->pending.mode;
    if (!currentMode) {
        currentMode = modes.constFirst();
    }
    setModesInternal(modes, currentMode);

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
        if (plane && (!plane->formats().contains(DRM_FORMAT_ARGB8888) || !plane->formats().value(DRM_FORMAT_ARGB8888).contains(DRM_FORMAT_MOD_LINEAR))) {
            m_pipeline->setCursor(nullptr);
            m_setCursorSuccessful = false;
            return;
        }
        m_cursor = QSharedPointer<DumbSwapchain>::create(m_gpu, m_gpu->cursorSize(), plane ? DRM_FORMAT_ARGB8888 : DRM_FORMAT_XRGB8888, QImage::Format::Format_ARGB32_Premultiplied);
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

QList<QSharedPointer<OutputMode>> DrmOutput::getModes() const
{
    const auto drmModes = m_pipeline->connector()->modes();

    QList<QSharedPointer<OutputMode>> ret;
    ret.reserve(drmModes.count());
    for (const QSharedPointer<DrmConnectorMode> &drmMode : drmModes) {
        ret.append(drmMode);
    }
    return ret;
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
    const QList<QSharedPointer<OutputMode>> modes = getModes();

    if (m_pipeline->pending.crtc) {
        const auto currentMode = m_pipeline->connector()->findMode(m_pipeline->pending.crtc->queryCurrentMode());
        if (currentMode != m_pipeline->pending.mode) {
            // DrmConnector::findCurrentMode might fail
            m_pipeline->pending.mode = currentMode ? currentMode : m_pipeline->connector()->modes().constFirst();
            if (m_gpu->testPendingConfiguration()) {
                m_pipeline->applyPendingChanges();
                m_renderLoop->setRefreshRate(m_pipeline->pending.mode->refreshRate());
            } else {
                qCWarning(KWIN_DRM) << "Setting changed mode failed!";
                m_pipeline->revertPendingChanges();
            }
        }
    }

    QSharedPointer<OutputMode> currentMode = m_pipeline->pending.mode;
    if (!currentMode) {
        currentMode = modes.constFirst();
    }

    setModesInternal(modes, currentMode);
}

bool DrmOutput::present()
{
    RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_renderLoop);
    if (m_pipeline->pending.syncMode != renderLoopPrivate->presentMode) {
        m_pipeline->pending.syncMode = renderLoopPrivate->presentMode;
        if (DrmPipeline::commitPipelines({m_pipeline}, DrmPipeline::CommitMode::Test)) {
            m_pipeline->applyPendingChanges();
        } else {
            m_pipeline->revertPendingChanges();
        }
    }
    bool modeset = gpu()->needsModeset();
    if (modeset ? m_pipeline->maybeModeset() : m_pipeline->present()) {
        Q_EMIT outputChange(m_pipeline->pending.layer->currentDamage());
        return true;
    } else if (!modeset) {
        qCWarning(KWIN_DRM) << "Presentation failed!" << strerror(errno);
        frameFailed();
    }
    return false;
}

DrmConnector *DrmOutput::connector() const
{
    return m_connector;
}

DrmPipeline *DrmOutput::pipeline() const
{
    return m_pipeline;
}

bool DrmOutput::queueChanges(const OutputConfiguration &config)
{
    static bool valid;
    static int envOnlySoftwareRotations = qEnvironmentVariableIntValue("KWIN_DRM_SW_ROTATIONS_ONLY", &valid) == 1 || !valid;

    const auto props = config.constChangeSet(this);
    m_pipeline->pending.active = props->enabled;
    const auto modelist = m_connector->modes();
    const auto it = std::find_if(modelist.begin(), modelist.end(), [&props](const auto &mode) {
        return mode->size() == props->modeSize && mode->refreshRate() == props->refreshRate;
    });
    if (it == modelist.end()) {
        qCWarning(KWIN_DRM).nospace() << "Could not find mode " << props->modeSize << "@" << props->refreshRate << " for output " << this;
        return false;
    }
    m_pipeline->pending.mode = *it;
    m_pipeline->pending.overscan = props->overscan;
    m_pipeline->pending.rgbRange = props->rgbRange;
    m_pipeline->pending.renderOrientation = outputToPlaneTransform(props->transform);
    if (!envOnlySoftwareRotations && m_gpu->atomicModeSetting()) {
        m_pipeline->pending.bufferOrientation = m_pipeline->pending.renderOrientation;
    }
    m_pipeline->pending.enabled = props->enabled;
    return true;
}

void DrmOutput::applyQueuedChanges(const OutputConfiguration &config)
{
    if (!m_connector->isConnected()) {
        return;
    }
    Q_EMIT aboutToChange();
    m_pipeline->applyPendingChanges();

    auto props = config.constChangeSet(this);
    setEnabled(props->enabled && m_pipeline->pending.crtc);
    if (!isEnabled() && m_pipeline->needsModeset()) {
        m_gpu->maybeModeset();
    }
    moveTo(props->pos);
    setScale(props->scale);
    setTransformInternal(props->transform);

    const auto &mode = m_pipeline->pending.mode;
    setCurrentModeInternal(mode);
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

bool DrmOutput::usesSoftwareCursor() const
{
    return !m_setCursorSuccessful || !m_moveCursorSuccessful;
}

DrmOutputLayer *DrmOutput::outputLayer() const
{
    return m_pipeline->pending.layer.data();
}

void DrmOutput::setColorTransformation(const QSharedPointer<ColorTransformation> &transformation)
{
    m_pipeline->pending.colorTransformation = transformation;
    if (DrmPipeline::commitPipelines({m_pipeline}, DrmPipeline::CommitMode::Test)) {
        m_pipeline->applyPendingChanges();
        m_renderLoop->scheduleRepaint();
    } else {
        m_pipeline->revertPendingChanges();
    }
}
}
