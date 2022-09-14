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

#include "core/outputconfiguration.h"
#include "core/renderloop.h"
#include "core/renderloop_p.h"
#include "core/session.h"
#include "cursor.h"
#include "drm_dumb_buffer.h"
#include "drm_dumb_swapchain.h"
#include "drm_egl_backend.h"
#include "drm_layer.h"
#include "drm_logging.h"
#include "kwinglutils.h"
#include "wayland/drmleasedevice_v1_interface.h"
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

DrmOutput::DrmOutput(DrmPipeline *pipeline, KWaylandServer::DrmLeaseDeviceV1Interface *leaseDevice)
    : DrmAbstractOutput(pipeline->connector()->gpu())
    , m_pipeline(pipeline)
    , m_connector(pipeline->connector())
{
    m_pipeline->setOutput(this);
    const auto conn = m_pipeline->connector();
    m_renderLoop->setRefreshRate(m_pipeline->mode()->refreshRate());

    Capabilities capabilities = Capability::Dpms;
    State initialState;

    if (conn->hasOverscan()) {
        capabilities |= Capability::Overscan;
        initialState.overscan = conn->overscan();
    }
    if (conn->vrrCapable()) {
        capabilities |= Capability::Vrr;
        setVrrPolicy(RenderLoop::VrrPolicy::Automatic);
    }
    if (conn->hasRgbRange()) {
        capabilities |= Capability::RgbRange;
        initialState.rgbRange = conn->rgbRange();
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
        .nonDesktop = conn->isNonDesktop(),
    });

    initialState.modes = getModes();
    initialState.currentMode = m_pipeline->mode();
    if (!initialState.currentMode) {
        initialState.currentMode = initialState.modes.constFirst();
    }

    setState(initialState);

    m_turnOffTimer.setSingleShot(true);
    m_turnOffTimer.setInterval(dimAnimationTime());
    connect(&m_turnOffTimer, &QTimer::timeout, this, [this] {
        setDrmDpmsMode(DpmsMode::Off);
    });

    if (conn->isNonDesktop()) {
        m_offer = std::make_unique<KWaylandServer::DrmLeaseConnectorV1Interface>(
            leaseDevice,
            conn->id(),
            conn->modelName(),
            QStringLiteral("%1 %2").arg(conn->edid()->manufacturerString(), conn->modelName()));
    } else {
        connect(Cursors::self(), &Cursors::currentCursorChanged, this, &DrmOutput::updateCursor);
        connect(Cursors::self(), &Cursors::hiddenChanged, this, &DrmOutput::updateCursor);
        connect(Cursors::self(), &Cursors::positionChanged, this, &DrmOutput::moveCursor);
    }
}

DrmOutput::~DrmOutput()
{
    m_pipeline->setOutput(nullptr);
}

bool DrmOutput::addLeaseObjects(QVector<uint32_t> &objectList)
{
    Q_ASSERT(m_offer);
    if (!m_pipeline->crtc()) {
        qCWarning(KWIN_DRM) << "Can't lease connector: No suitable crtc available";
        return false;
    }
    qCDebug(KWIN_DRM) << "adding connector" << m_pipeline->connector()->id() << "to lease";
    objectList << m_pipeline->connector()->id();
    objectList << m_pipeline->crtc()->id();
    if (m_pipeline->crtc()->primaryPlane()) {
        objectList << m_pipeline->crtc()->primaryPlane()->id();
    }
    return true;
}

void DrmOutput::leased(KWaylandServer::DrmLeaseV1Interface *lease)
{
    Q_ASSERT(m_offer);
    m_lease = lease;
}

void DrmOutput::leaseEnded()
{
    Q_ASSERT(m_offer);
    qCDebug(KWIN_DRM) << "ended lease for connector" << m_pipeline->connector()->id();
    m_lease = nullptr;
}

KWaylandServer::DrmLeaseV1Interface *DrmOutput::lease() const
{
    return m_lease;
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
    bool rendered = false;
    const QMatrix4x4 monitorMatrix = logicalToNativeMatrix(geometry(), scale(), transform());
    const QRect cursorRect = monitorMatrix.mapRect(cursor->geometry());
    if (cursorRect.width() <= m_gpu->cursorSize().width() && cursorRect.height() <= m_gpu->cursorSize().height()) {
        if (const auto beginInfo = layer->beginFrame()) {
            const auto &[renderTarget, repaint] = beginInfo.value();
            if (dynamic_cast<EglGbmBackend *>(m_gpu->platform()->renderBackend())) {
                renderCursorOpengl(renderTarget, cursor->geometry().size() * scale());
            } else {
                renderCursorQPainter(renderTarget);
            }
            rendered = layer->endFrame(infiniteRegion(), infiniteRegion());
        }
    }
    if (!rendered) {
        if (layer->isVisible()) {
            layer->setVisible(false);
            m_pipeline->setCursor();
        }
        m_setCursorSuccessful = false;
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

QList<std::shared_ptr<OutputMode>> DrmOutput::getModes() const
{
    const auto drmModes = m_pipeline->connector()->modes();

    QList<std::shared_ptr<OutputMode>> ret;
    ret.reserve(drmModes.count());
    for (const auto &drmMode : drmModes) {
        ret.append(drmMode);
    }
    return ret;
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
        updateDpmsMode(mode);
        return true;
    }
    m_pipeline->setActive(active);
    if (DrmPipeline::commitPipelines({m_pipeline}, active ? DrmPipeline::CommitMode::TestAllowModeset : DrmPipeline::CommitMode::CommitModeset) == DrmPipeline::Error::None) {
        m_pipeline->applyPendingChanges();
        updateDpmsMode(mode);
        if (active) {
            m_gpu->platform()->checkOutputsAreOn();
            m_renderLoop->uninhibit();
            m_renderLoop->scheduleRepaint();
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
    State next = m_state;
    next.modes = getModes();

    if (m_pipeline->crtc()) {
        const auto currentMode = m_pipeline->connector()->findMode(m_pipeline->crtc()->queryCurrentMode());
        if (currentMode != m_pipeline->mode()) {
            // DrmConnector::findCurrentMode might fail
            m_pipeline->setMode(currentMode ? currentMode : m_pipeline->connector()->modes().constFirst());
            if (m_gpu->testPendingConfiguration() == DrmPipeline::Error::None) {
                m_pipeline->applyPendingChanges();
                m_renderLoop->setRefreshRate(m_pipeline->mode()->refreshRate());
            } else {
                qCWarning(KWIN_DRM) << "Setting changed mode failed!";
                m_pipeline->revertPendingChanges();
            }
        }
    }

    next.currentMode = m_pipeline->mode();
    if (!next.currentMode) {
        next.currentMode = next.modes.constFirst();
    }

    setState(next);
}

void DrmOutput::updateDpmsMode(DpmsMode dpmsMode)
{
    State next = m_state;
    next.dpmsMode = dpmsMode;
    setState(next);
}

bool DrmOutput::present()
{
    RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_renderLoop.get());
    if (m_pipeline->syncMode() != renderLoopPrivate->presentMode) {
        m_pipeline->setSyncMode(renderLoopPrivate->presentMode);
        if (DrmPipeline::commitPipelines({m_pipeline}, DrmPipeline::CommitMode::Test) == DrmPipeline::Error::None) {
            m_pipeline->applyPendingChanges();
        } else {
            m_pipeline->revertPendingChanges();
        }
    }
    const bool needsModeset = gpu()->needsModeset();
    bool success;
    if (needsModeset) {
        success = m_pipeline->maybeModeset();
    } else {
        DrmPipeline::Error err = m_pipeline->present();
        success = err == DrmPipeline::Error::None;
        if (err == DrmPipeline::Error::InvalidArguments) {
            QTimer::singleShot(0, m_gpu->platform(), &DrmBackend::updateOutputs);
        }
    }
    if (success) {
        Q_EMIT outputChange(m_pipeline->primaryLayer()->currentDamage());
        return true;
    } else if (!needsModeset) {
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
    m_pipeline->setMode(std::static_pointer_cast<DrmConnectorMode>(props->mode));
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

    State next = m_state;
    next.enabled = props->enabled && m_pipeline->crtc();
    next.position = props->pos;
    next.scale = props->scale;
    next.transform = props->transform;
    next.currentMode = m_pipeline->mode();
    next.overscan = m_pipeline->overscan();
    next.rgbRange = m_pipeline->rgbRange();

    setState(next);
    setVrrPolicy(props->vrrPolicy);

    if (!isEnabled() && m_pipeline->needsModeset()) {
        m_gpu->maybeModeset();
    }

    m_renderLoop->setRefreshRate(refreshRate());
    m_renderLoop->scheduleRepaint();

    Q_EMIT changed();

    if (isEnabled() && dpmsMode() == DpmsMode::On) {
        m_gpu->platform()->turnOutputsOn();
    }

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

void DrmOutput::setColorTransformation(const std::shared_ptr<ColorTransformation> &transformation)
{
    m_pipeline->setColorTransformation(transformation);
    if (DrmPipeline::commitPipelines({m_pipeline}, DrmPipeline::CommitMode::Test) == DrmPipeline::Error::None) {
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
        m_cursorTexture = std::make_unique<GLTexture>(img);
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
