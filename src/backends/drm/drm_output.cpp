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
#include "drm_dumb_buffer.h"
#include "drm_layer.h"
#include "dumb_swapchain.h"
#include "egl_gbm_backend.h"
#include "kwinglutils.h"
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
    m_renderLoop->setRefreshRate(m_pipeline->mode()->refreshRate());

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
    QSharedPointer<OutputMode> currentMode = m_pipeline->mode();
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

void DrmOutput::updateCursor()
{
    static bool valid;
    static const bool forceSoftwareCursor = qEnvironmentVariableIntValue("KWIN_FORCE_SW_CURSOR", &valid) == 1 && valid;
    // hardware cursors are broken with the NVidia proprietary driver
    if (forceSoftwareCursor || (!valid && m_gpu->isNVidia())) {
        m_setCursorSuccessful = false;
        return;
    }
    const auto layer = m_pipeline->cursorLayer();
    if (!m_pipeline->crtc() || !layer) {
        return;
    }
    const Cursor *cursor = Cursors::self()->currentCursor();
    if (!cursor || cursor->image().isNull() || Cursors::self()->isCursorHidden()) {
        if (layer->isVisible()) {
            layer->setVisible(false);
            m_pipeline->setCursor();
        }
        return;
    }
    const QMatrix4x4 monitorMatrix = logicalToNativeMatrix(geometry(), scale(), transform());
    const QRect cursorRect = monitorMatrix.mapRect(cursor->geometry());
    if (cursorRect.width() > m_gpu->cursorSize().width() || cursorRect.height() > m_gpu->cursorSize().height()) {
        if (layer->isVisible()) {
            layer->setVisible(false);
            m_pipeline->setCursor();
        }
        m_setCursorSuccessful = false;
        return;
    }
    const auto [renderTarget, repaint] = layer->beginFrame();
    if (dynamic_cast<EglGbmBackend *>(m_gpu->platform()->renderBackend())) {
        renderCursorOpengl(renderTarget, cursor->geometry().size() * scale());
    } else {
        renderCursorQPainter(renderTarget);
    }
    bool rendered = layer->endFrame(infiniteRegion(), infiniteRegion());
    if (!rendered) {
        m_setCursorSuccessful = false;
        layer->setVisible(false);
        return;
    }

    const QSize surfaceSize = m_gpu->cursorSize() / scale();
    const QRect layerRect = monitorMatrix.mapRect(QRect(cursor->geometry().topLeft(), surfaceSize));
    layer->setPosition(layerRect.topLeft());
    layer->setVisible(cursor->geometry().intersects(geometry()));
    if (layer->isVisible()) {
        m_setCursorSuccessful = m_pipeline->setCursor(logicalToNativeMatrix(QRect(QPoint(), layerRect.size()), scale(), transform()).map(cursor->hotspot()));
        layer->setVisible(m_setCursorSuccessful);
    }
}

void DrmOutput::moveCursor()
{
    if (!m_setCursorSuccessful || !m_pipeline->crtc()) {
        return;
    }
    const auto layer = m_pipeline->cursorLayer();
    Cursor *cursor = Cursors::self()->currentCursor();
    if (!cursor || cursor->image().isNull() || Cursors::self()->isCursorHidden() || !cursor->geometry().intersects(geometry())) {
        if (layer->isVisible()) {
            layer->setVisible(false);
            m_pipeline->setCursor();
        }
        return;
    }
    const QMatrix4x4 monitorMatrix = logicalToNativeMatrix(geometry(), scale(), transform());
    const QSize surfaceSize = m_gpu->cursorSize() / scale();
    const QRect cursorRect = monitorMatrix.mapRect(QRect(cursor->geometry().topLeft(), surfaceSize));
    layer->setVisible(true);
    layer->setPosition(cursorRect.topLeft());
    m_moveCursorSuccessful = m_pipeline->moveCursor();
    layer->setVisible(m_moveCursorSuccessful);
    if (!m_moveCursorSuccessful) {
        m_pipeline->setCursor();
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
    m_pipeline->setActive(active);
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

    if (m_pipeline->crtc()) {
        const auto currentMode = m_pipeline->connector()->findMode(m_pipeline->crtc()->queryCurrentMode());
        if (currentMode != m_pipeline->mode()) {
            // DrmConnector::findCurrentMode might fail
            m_pipeline->setMode(currentMode ? currentMode : m_pipeline->connector()->modes().constFirst());
            if (m_gpu->testPendingConfiguration()) {
                m_pipeline->applyPendingChanges();
                m_renderLoop->setRefreshRate(m_pipeline->mode()->refreshRate());
            } else {
                qCWarning(KWIN_DRM) << "Setting changed mode failed!";
                m_pipeline->revertPendingChanges();
            }
        }
    }

    QSharedPointer<OutputMode> currentMode = m_pipeline->mode();
    if (!currentMode) {
        currentMode = modes.constFirst();
    }

    setModesInternal(modes, currentMode);
}

bool DrmOutput::present()
{
    RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_renderLoop);
    if (m_pipeline->syncMode() != renderLoopPrivate->presentMode) {
        m_pipeline->setSyncMode(renderLoopPrivate->presentMode);
        if (DrmPipeline::commitPipelines({m_pipeline}, DrmPipeline::CommitMode::Test)) {
            m_pipeline->applyPendingChanges();
        } else {
            m_pipeline->revertPendingChanges();
        }
    }
    bool modeset = gpu()->needsModeset();
    if (modeset ? m_pipeline->maybeModeset() : m_pipeline->present()) {
        Q_EMIT outputChange(m_pipeline->primaryLayer()->currentDamage());
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
    m_pipeline->setActive(props->enabled);
    const auto modelist = m_connector->modes();
    const auto it = std::find_if(modelist.begin(), modelist.end(), [&props](const auto &mode) {
        return mode->size() == props->modeSize && mode->refreshRate() == props->refreshRate;
    });
    if (it == modelist.end()) {
        qCWarning(KWIN_DRM).nospace() << "Could not find mode " << props->modeSize << "@" << props->refreshRate << " for output " << this;
        return false;
    }
    m_pipeline->setMode(*it);
    m_pipeline->setOverscan(props->overscan);
    m_pipeline->setRgbRange(props->rgbRange);
    m_pipeline->setRenderOrientation(outputToPlaneTransform(props->transform));
    if (!envOnlySoftwareRotations && m_gpu->atomicModeSetting()) {
        m_pipeline->setBufferOrientation(m_pipeline->renderOrientation());
    }
    m_pipeline->setEnable(props->enabled);
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
    setEnabled(props->enabled && m_pipeline->crtc());
    if (!isEnabled() && m_pipeline->needsModeset()) {
        m_gpu->maybeModeset();
    }
    moveTo(props->pos);
    setScale(props->scale);
    setTransformInternal(props->transform);

    const auto mode = m_pipeline->mode();
    setCurrentModeInternal(mode);
    m_renderLoop->setRefreshRate(mode->refreshRate());
    setOverscanInternal(m_pipeline->overscan());
    setRgbRangeInternal(m_pipeline->rgbRange());
    setVrrPolicy(props->vrrPolicy);

    m_renderLoop->scheduleRepaint();
    Q_EMIT changed();

    updateCursor();
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
    return m_pipeline->primaryLayer();
}

void DrmOutput::setColorTransformation(const QSharedPointer<ColorTransformation> &transformation)
{
    m_pipeline->setColorTransformation(transformation);
    if (DrmPipeline::commitPipelines({m_pipeline}, DrmPipeline::CommitMode::Test)) {
        m_pipeline->applyPendingChanges();
        m_renderLoop->scheduleRepaint();
    } else {
        m_pipeline->revertPendingChanges();
    }
}

void DrmOutput::renderCursorOpengl(const RenderTarget &renderTarget, const QSize &cursorSize)
{
    auto allocateTexture = [this]() {
        const QImage img = Cursors::self()->currentCursor()->image();
        if (img.isNull()) {
            m_cursorTextureDirty = false;
            return;
        }
        m_cursorTexture.reset(new GLTexture(img));
        m_cursorTexture->setWrapMode(GL_CLAMP_TO_EDGE);
        m_cursorTextureDirty = false;
    };

    if (!m_cursorTexture) {
        allocateTexture();

        // handle shape update on case cursor image changed
        connect(Cursors::self(), &Cursors::currentCursorChanged, this, [this]() {
            m_cursorTextureDirty = true;
        });
    } else if (m_cursorTextureDirty) {
        const QImage image = Cursors::self()->currentCursor()->image();
        if (image.size() == m_cursorTexture->size()) {
            m_cursorTexture->update(image);
            m_cursorTextureDirty = false;
        } else {
            allocateTexture();
        }
    }

    QMatrix4x4 mvp;
    mvp.ortho(QRect(QPoint(), renderTarget.size()));

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_cursorTexture->bind();
    ShaderBinder binder(ShaderTrait::MapTexture);
    binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
    m_cursorTexture->render(QRect(0, 0, cursorSize.width(), cursorSize.height()));
    m_cursorTexture->unbind();
    glDisable(GL_BLEND);
}

void DrmOutput::renderCursorQPainter(const RenderTarget &renderTarget)
{
    const auto layer = m_pipeline->cursorLayer();
    const Cursor *cursor = Cursors::self()->currentCursor();
    const QImage cursorImage = cursor->image();

    QImage *c = std::get<QImage *>(renderTarget.nativeHandle());
    c->setDevicePixelRatio(scale());
    c->fill(Qt::transparent);

    QPainter p;
    p.begin(c);
    p.setWorldTransform(logicalToNativeMatrix(cursor->rect(), 1, transform()).toTransform());
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(QPoint(0, 0), cursorImage);
    p.end();
}
}
