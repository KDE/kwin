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
#include "renderloop.h"
#include "renderloop_p.h"
#include "renderoutput.h"
#include "scene.h"
#include "screens.h"
#include "session.h"
#include "waylandoutputconfig.h"
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

DrmOutput::DrmOutput(const QVector<DrmConnector *> &connectors)
    : DrmAbstractOutput(connectors.first()->gpu())
    , m_connectors(connectors)
{
    for (const auto &connector : m_connectors) {
        m_pipelines << connector->pipeline();
        connector->pipeline()->setOutput(this);
        m_renderOutputs << QSharedPointer<DrmRenderOutput>::create(this, connector->pipeline());
    }
    const auto conn0 = m_connectors.first();
    m_renderLoop->setRefreshRate(conn0->pipeline()->pending.mode->refreshRate());
    setSubPixelInternal(conn0->subpixel());
    setInternal(conn0->isInternal());
    setCapabilityInternal(DrmOutput::Capability::Dpms);
    if (m_connectors.size() == 1 && conn0->hasOverscan()) {
        setCapabilityInternal(Capability::Overscan);
        setOverscanInternal(conn0->overscan());
    }
    if (std::all_of(m_connectors.begin(), m_connectors.end(), [](const auto &conn) {
            return conn->vrrCapable();
        })) {
        setCapabilityInternal(Capability::Vrr);
        setVrrPolicy(RenderLoop::VrrPolicy::Automatic);
    }
    if (std::all_of(m_connectors.begin(), m_connectors.end(), [](const auto &conn) {
            return conn->hasRgbRange();
        })) {
        setCapabilityInternal(Capability::RgbRange);
        setRgbRangeInternal(conn0->rgbRange());
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
    for (const auto &pipeline : qAsConst(m_pipelines)) {
        pipeline->setOutput(nullptr);
    }
}

QVector<AbstractWaylandOutput::Mode> DrmOutput::getModes() const
{
    bool modeFound = false;
    QVector<Mode> modes;
    const auto modelist = m_pipelines.constFirst()->connector()->modes();
    modes.reserve(modelist.count());
    for (int i = 0; i < modelist.count(); ++i) {
        Mode mode;
        // compare the actual mode objects, not the pointers!
        if (*modelist[i] == *m_pipelines.constFirst()->pending.mode) {
            mode.flags |= ModeFlag::Current;
            modeFound = true;
        }
        if (modelist[i]->nativeMode()->type & DRM_MODE_TYPE_PREFERRED) {
            mode.flags |= ModeFlag::Preferred;
        }

        mode.id = i;
        mode.size = m_pipelines.size() > 1 ? m_connectors.first()->totalTiledOutputSize() : modelist[i]->size();
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
    const auto conn = m_connectors.first();
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
    for (const auto &pipeline : qAsConst(m_pipelines)) {
        pipeline->pending.active = active;
    }
    if (DrmPipeline::commitPipelines(m_pipelines, active ? DrmPipeline::CommitMode::Test : DrmPipeline::CommitMode::CommitModeset)) {
        applyPipelineChanges();
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
        revertPipelineChanges();
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
    setModes(getModes());
    if (m_pipelines.constFirst()->pending.crtc) {
        bool needsCommit = false;
        for (const auto &connector : m_connectors) {
            const auto currentMode = connector->findMode(connector->pipeline()->pending.crtc->queryCurrentMode());
            if (currentMode != connector->pipeline()->pending.mode) {
                needsCommit = true;
                // DrmConnector::findCurrentMode might fail
                connector->pipeline()->pending.mode = currentMode ? currentMode : connector->modes().constFirst();
            }
        }
        if (needsCommit) {
            if (m_gpu->testPendingConfiguration(DrmGpu::TestMode::TestWithCrtcReallocation)) {
                applyPipelineChanges();
                const auto modeSize = m_connectors.size() > 1 ? m_connectors.first()->totalTiledOutputSize() : m_pipelines.constFirst()->pending.mode->size();
                setCurrentModeInternal(modeSize, m_pipelines.constFirst()->pending.mode->refreshRate());
                m_renderLoop->setRefreshRate(m_pipelines.constFirst()->pending.mode->refreshRate());
            } else {
                qCWarning(KWIN_DRM) << "Setting changed mode failed!";
                revertPipelineChanges();
            }
        }
    }
}

bool DrmOutput::present()
{
    RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_renderLoop);
    bool needsTest = false;
    for (const auto &pipeline : qAsConst(m_pipelines)) {
        needsTest |= pipeline->pending.syncMode != renderLoopPrivate->presentMode;
        pipeline->pending.syncMode = renderLoopPrivate->presentMode;
    }
    if (needsTest) {
        if (DrmPipeline::commitPipelines(m_pipelines, DrmPipeline::CommitMode::Test)) {
            applyPipelineChanges();
        } else {
            revertPipelineChanges();
            setVrrPolicy(RenderLoop::VrrPolicy::Never);
        }
    }
    if (DrmPipeline::presentPipelines(m_pipelines)) {
        QRegion damage;
        for (const auto &pipeline : qAsConst(m_pipelines)) {
            damage |= pipeline->pending.layer->currentDamage();
        }
        Q_EMIT outputChange(damage);
        return true;
    } else {
        qCWarning(KWIN_DRM) << "Presentation failed!" << strerror(errno);
        frameFailed();
        return false;
    }
}

int DrmOutput::gammaRampSize() const
{
    return m_pipelines.constFirst()->pending.crtc ? m_pipelines.constFirst()->pending.crtc->gammaRampSize() : 256;
}

bool DrmOutput::setGammaRamp(const GammaRamp &gamma)
{
    const auto gammaRamp = QSharedPointer<DrmGammaRamp>::create(m_gpu, gamma);
    for (const auto &pipeline : qAsConst(m_pipelines)) {
        pipeline->pending.gamma = gammaRamp;
    }
    if (DrmPipeline::commitPipelines(m_pipelines, DrmPipeline::CommitMode::Test)) {
        applyPipelineChanges();
        m_renderLoop->scheduleRepaint();
        return true;
    } else {
        revertPipelineChanges();
        return false;
    }
}

bool DrmOutput::queueChanges(const WaylandOutputConfig &config)
{
    static bool valid;
    static int envOnlySoftwareRotations = qEnvironmentVariableIntValue("KWIN_DRM_SW_ROTATIONS_ONLY", &valid) == 1 || !valid;

    const auto props = config.constChangeSet(this);
    for (const auto &pipeline : qAsConst(m_pipelines)) {
        pipeline->pending.active = props->enabled;
        const auto modelist = pipeline->connector()->modes();
        const auto it = std::find_if(modelist.begin(), modelist.end(), [&props](const auto &mode) {
            return mode->size() == props->modeSize && mode->refreshRate() == props->refreshRate;
        });
        if (it == modelist.end()) {
            qCWarning(KWIN_DRM).nospace() << "Could not find mode " << props->modeSize << "@" << props->refreshRate << " for output " << this;
            revertPipelineChanges();
            return false;
        }
        pipeline->pending.mode = *it;
        pipeline->pending.overscan = props->overscan;
        pipeline->pending.rgbRange = props->rgbRange;
        pipeline->pending.sourceTransformation = outputToPlaneTransform(props->transform);
        if (!envOnlySoftwareRotations && m_gpu->atomicModeSetting() && m_pipelines.size() == 1) {
            pipeline->pending.bufferTransformation = pipeline->pending.sourceTransformation;
        }
        pipeline->pending.enabled = props->enabled;
    }
    return true;
}

void DrmOutput::applyQueuedChanges(const WaylandOutputConfig &config)
{
    Q_EMIT aboutToChange();
    applyPipelineChanges();

    auto props = config.constChangeSet(this);
    setEnabled(props->enabled && m_pipelines.constFirst()->pending.crtc);
    if (!isEnabled() && std::any_of(m_pipelines.begin(), m_pipelines.end(), [](const auto &pipeline){return pipeline->needsModeset();})) {
        m_gpu->maybeModeset();
    }
    moveTo(props->pos);
    setScale(props->scale);
    setTransformInternal(props->transform);

    const auto &mode = m_pipelines.constFirst()->pending.mode;
    const auto modeSize = m_connectors.size() > 1 ? m_connectors.first()->totalTiledOutputSize() : mode->size();
    setCurrentModeInternal(modeSize, mode->refreshRate());
    m_renderLoop->setRefreshRate(mode->refreshRate());
    setOverscanInternal(m_pipelines.constFirst()->pending.overscan);
    setRgbRangeInternal(m_pipelines.constFirst()->pending.rgbRange);
    setVrrPolicy(props->vrrPolicy);

    m_renderLoop->scheduleRepaint();
    Q_EMIT changed();
}

void DrmOutput::revertQueuedChanges()
{
    revertPipelineChanges();
}

QVector<QSharedPointer<RenderOutput>> DrmOutput::renderOutputs() const
{
    return m_renderOutputs;
}

QVector<DrmConnector *> DrmOutput::connectors() const
{
    return m_connectors;
}

void DrmOutput::applyPipelineChanges()
{
    for (const auto &pipeline : qAsConst(m_pipelines)) {
        pipeline->applyPendingChanges();
    }
}

void DrmOutput::revertPipelineChanges()
{
    for (const auto &pipeline : qAsConst(m_pipelines)) {
        pipeline->revertPendingChanges();
    }
}

void DrmOutput::pageFlipped(std::chrono::nanoseconds timestamp) const
{
    bool allFlipped = std::none_of(m_pipelines.constBegin(), m_pipelines.constEnd(), [](const auto &pipeline) {
        return pipeline->pageflipPending();
    });
    if (allFlipped) {
        DrmAbstractOutput::pageFlipped(timestamp);
    }
}

DrmRenderOutput::DrmRenderOutput(DrmOutput *output, DrmPipeline *pipeline)
    : DrmAbstractRenderOutput(output)
    , m_pipeline(pipeline)
{
    connect(output, &DrmOutput::geometryChanged, this, &DrmRenderOutput::geometryChanged);
    connect(Cursors::self(), &Cursors::currentCursorChanged, this, &DrmRenderOutput::updateCursor);
    connect(Cursors::self(), &Cursors::hiddenChanged, this, &DrmRenderOutput::updateCursor);
    connect(Cursors::self(), &Cursors::positionChanged, this, &DrmRenderOutput::moveCursor);
}

QRect DrmRenderOutput::geometry() const
{
    if (m_pipeline->connector()->isTiled()) {
        const auto relativePos = m_pipeline->connector()->tilePosition();
        const auto geom = m_pipeline->output()->geometry();
        return QRect(geom.topLeft() + QPoint(relativePos.x() * geom.size().width(), relativePos.y() * geom.size().height()), geom.size());
    } else {
        return m_pipeline->output()->geometry();
    }
}

bool DrmRenderOutput::usesSoftwareCursor() const
{
    return !(m_setCursorSuccessful && m_moveCursorSuccessful);
}

OutputLayer *DrmRenderOutput::layer() const
{
    return m_pipeline->pending.layer.get();
}

static bool isCursorSpriteCompatible(const QImage *buffer, const QImage *sprite)
{
    // Note that we need compare the rects in the device independent pixels because the
    // buffer and the cursor sprite image may have different scale factors.
    const QRect bufferRect(QPoint(0, 0), buffer->size() / buffer->devicePixelRatio());
    const QRect spriteRect(QPoint(0, 0), sprite->size() / sprite->devicePixelRatio());

    return bufferRect.contains(spriteRect);
}

void DrmRenderOutput::updateCursor()
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
        m_cursor = QSharedPointer<DumbSwapchain>::create(m_pipeline->gpu(), m_pipeline->gpu()->cursorSize(), plane ? DRM_FORMAT_ARGB8888 : DRM_FORMAT_XRGB8888, QImage::Format::Format_ARGB32_Premultiplied);
        if (!m_cursor || m_cursor->isEmpty()) {
            m_pipeline->setCursor(nullptr);
            m_setCursorSuccessful = false;
            return;
        }
    }
    m_cursor->releaseBuffer(m_cursor->currentBuffer());
    m_cursor->acquireBuffer();

    const auto output = m_pipeline->output();
    QImage *c = m_cursor->currentBuffer()->image();
    c->setDevicePixelRatio(output->scale());
    if (!isCursorSpriteCompatible(c, &cursorImage)) {
        // If the cursor image is too big, fall back to rendering the software cursor.
        m_pipeline->setCursor(nullptr);
        m_setCursorSuccessful = false;
        return;
    }
    c->fill(Qt::transparent);
    QPainter p;
    p.begin(c);
    p.setWorldTransform(output->logicalToNativeMatrix(cursor->rect(), 1, output->transform()).toTransform());
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(QPoint(0, 0), cursorImage);
    p.end();
    m_setCursorSuccessful = m_pipeline->setCursor(m_cursor->currentBuffer(), output->logicalToNativeMatrix(cursor->rect(), output->scale(), output->transform()).map(cursor->hotspot()));
    moveCursor();
}

void DrmRenderOutput::moveCursor()
{
    if (!m_setCursorSuccessful || !m_pipeline->pending.crtc) {
        return;
    }
    Cursor *cursor = Cursors::self()->currentCursor();
    const auto output = m_pipeline->output();
    const QMatrix4x4 monitorMatrix = output->logicalToNativeMatrix(geometry(), output->scale(), output->transform());
    const QMatrix4x4 hotspotMatrix = output->logicalToNativeMatrix(cursor->rect(), output->scale(), output->transform());
    m_moveCursorSuccessful = m_pipeline->moveCursor(monitorMatrix.map(cursor->pos()) - hotspotMatrix.map(cursor->hotspot()));
    if (!m_moveCursorSuccessful) {
        m_pipeline->setCursor(nullptr);
    }
}

}
