/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_output.h"
#include "drm_backend.h"
#include "drm_connector.h"
#include "drm_crtc.h"
#include "drm_gpu.h"
#include "drm_pipeline.h"

#include "core/colortransformation.h"
#include "core/iccprofile.h"
#include "core/outputconfiguration.h"
#include "core/renderloop.h"
#include "core/renderloop_p.h"
#include "drm_layer.h"
#include "drm_logging.h"
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

DrmOutput::DrmOutput(const std::shared_ptr<DrmConnector> &conn)
    : DrmAbstractOutput(conn->gpu())
    , m_pipeline(conn->pipeline())
    , m_connector(conn)
{
    RenderLoopPrivate::get(m_renderLoop.get())->canDoTearing = gpu()->asyncPageflipSupported();
    m_pipeline->setOutput(this);
    m_renderLoop->setRefreshRate(m_pipeline->mode()->refreshRate());

    Capabilities capabilities = Capability::Dpms;
    State initialState;

    if (conn->overscan.isValid() || conn->underscan.isValid()) {
        capabilities |= Capability::Overscan;
        initialState.overscan = conn->overscan.isValid() ? conn->overscan.value() : conn->underscanVBorder.value();
    }
    if (conn->vrrCapable.isValid() && conn->vrrCapable.value()) {
        capabilities |= Capability::Vrr;
        setVrrPolicy(RenderLoop::VrrPolicy::Automatic);
    }
    if (conn->broadcastRGB.isValid()) {
        capabilities |= Capability::RgbRange;
        initialState.rgbRange = DrmConnector::broadcastRgbToRgbRange(conn->broadcastRGB.enumValue());
    }
    if (m_connector->hdrMetadata.isValid() && m_connector->edid() && m_connector->edid()->hdrMetadata() && m_connector->edid()->hdrMetadata()->supportsPQ) {
        capabilities |= Capability::HighDynamicRange;
    }
    if (m_connector->colorspace.isValid() && m_connector->colorspace.hasEnum(DrmConnector::Colorspace::BT2020_RGB)
        && m_connector->edid() && m_connector->edid()->hdrMetadata() && m_connector->edid()->hdrMetadata()->supportsBT2020) {
        capabilities |= Capability::WideColorGamut;
    }
    if (conn->isInternal()) {
        // TODO only set this if an orientation sensor is available?
        capabilities |= Capability::AutoRotation;
    }

    const Edid *edid = conn->edid();

    setInformation(Information{
        .name = conn->connectorName(),
        .manufacturer = edid->manufacturerString(),
        .model = conn->modelName(),
        .serialNumber = edid->serialNumber(),
        .eisaId = edid->eisaId(),
        .physicalSize = conn->physicalSize(),
        .edid = *edid,
        .subPixel = conn->subpixel(),
        .capabilities = capabilities,
        .panelOrientation = conn->panelOrientation.isValid() ? DrmConnector::toKWinTransform(conn->panelOrientation.enumValue()) : OutputTransform::Normal,
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
}

DrmOutput::~DrmOutput()
{
    m_pipeline->setOutput(nullptr);
}

bool DrmOutput::addLeaseObjects(QList<uint32_t> &objectList)
{
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

void DrmOutput::leased(DrmLease *lease)
{
    m_lease = lease;
}

void DrmOutput::leaseEnded()
{
    qCDebug(KWIN_DRM) << "ended lease for connector" << m_pipeline->connector()->id();
    m_lease = nullptr;
}

DrmLease *DrmOutput::lease() const
{
    return m_lease;
}

bool DrmOutput::updateCursorLayer()
{
    return m_pipeline->updateCursor();
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
    } else {
        if (m_turnOffTimer.isActive() || (mode != dpmsMode() && setDrmDpmsMode(mode))) {
            Q_EMIT wakeUp();
        }
        m_turnOffTimer.stop();
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
    if (!active) {
        gpu()->waitIdle();
    }
    m_pipeline->setActive(active);
    if (DrmPipeline::commitPipelines({m_pipeline}, active ? DrmPipeline::CommitMode::TestAllowModeset : DrmPipeline::CommitMode::CommitModeset) == DrmPipeline::Error::None) {
        m_pipeline->applyPendingChanges();
        updateDpmsMode(mode);
        if (active) {
            m_renderLoop->uninhibit();
            m_renderLoop->scheduleRepaint();
        } else {
            m_renderLoop->inhibit();
        }
        return true;
    } else {
        qCWarning(KWIN_DRM) << "Setting dpms mode failed!";
        m_pipeline->revertPendingChanges();
        return false;
    }
}

DrmPlane::Transformations outputToPlaneTransform(OutputTransform transform)
{
    using PlaneTrans = DrmPlane::Transformation;

    switch (transform.kind()) {
    case OutputTransform::Normal:
        return PlaneTrans::Rotate0;
    case OutputTransform::Flipped:
        return PlaneTrans::ReflectX | PlaneTrans::Rotate0;
    case OutputTransform::Rotated90:
        return PlaneTrans::Rotate90;
    case OutputTransform::Flipped90:
        return PlaneTrans::ReflectX | PlaneTrans::Rotate90;
    case OutputTransform::Rotated180:
        return PlaneTrans::Rotate180;
    case OutputTransform::Flipped180:
        return PlaneTrans::ReflectX | PlaneTrans::Rotate180;
    case OutputTransform::Rotated270:
        return PlaneTrans::Rotate270;
    case OutputTransform::Flipped270:
        return PlaneTrans::ReflectX | PlaneTrans::Rotate270;
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
    const auto type = DrmConnector::kwinToDrmContentType(contentType());
    if (m_pipeline->syncMode() != renderLoopPrivate->presentMode || type != m_pipeline->contentType()) {
        m_pipeline->setSyncMode(renderLoopPrivate->presentMode);
        m_pipeline->setContentType(type);
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
    return m_connector.get();
}

DrmPipeline *DrmOutput::pipeline() const
{
    return m_pipeline;
}

bool DrmOutput::queueChanges(const std::shared_ptr<OutputChangeSet> &props)
{
    const auto mode = props->mode.value_or(currentMode()).lock();
    if (!mode) {
        return false;
    }
    m_pipeline->setMode(std::static_pointer_cast<DrmConnectorMode>(mode));
    m_pipeline->setOverscan(props->overscan.value_or(m_pipeline->overscan()));
    m_pipeline->setRgbRange(props->rgbRange.value_or(m_pipeline->rgbRange()));
    m_pipeline->setRenderOrientation(outputToPlaneTransform(props->transform.value_or(transform())));
    m_pipeline->setEnable(props->enabled.value_or(m_pipeline->enabled()));
    m_pipeline->setBT2020(props->wideColorGamut.value_or(m_state.wideColorGamut));
    m_pipeline->setNamedTransferFunction(props->highDynamicRange.value_or(m_state.highDynamicRange) ? NamedTransferFunction::PerceptualQuantizer : NamedTransferFunction::sRGB);
    m_pipeline->setSdrBrightness(props->sdrBrightness.value_or(m_state.sdrBrightness));
    return true;
}

void DrmOutput::applyQueuedChanges(const std::shared_ptr<OutputChangeSet> &props)
{
    if (!m_connector->isConnected()) {
        return;
    }
    Q_EMIT aboutToChange();
    m_pipeline->applyPendingChanges();

    State next = m_state;
    next.enabled = props->enabled.value_or(m_state.enabled) && m_pipeline->crtc();
    next.position = props->pos.value_or(m_state.position);
    next.scale = props->scale.value_or(m_state.scale);
    next.transform = props->transform.value_or(m_state.transform);
    next.manualTransform = props->manualTransform.value_or(m_state.manualTransform);
    next.currentMode = m_pipeline->mode();
    next.overscan = m_pipeline->overscan();
    next.rgbRange = m_pipeline->rgbRange();
    next.highDynamicRange = props->highDynamicRange.value_or(m_state.highDynamicRange);
    next.sdrBrightness = props->sdrBrightness.value_or(m_state.sdrBrightness);
    next.wideColorGamut = props->wideColorGamut.value_or(m_state.wideColorGamut);
    next.autoRotatePolicy = props->autoRotationPolicy.value_or(m_state.autoRotatePolicy);
    if (props->iccProfilePath) {
        next.iccProfile = IccProfile::load(*props->iccProfilePath);
        m_pipeline->setIccProfile(next.iccProfile);
    }
    if (m_state.highDynamicRange != next.highDynamicRange || m_state.sdrBrightness != next.sdrBrightness || m_state.wideColorGamut != next.wideColorGamut || m_state.iccProfile != next.iccProfile) {
        m_renderLoop->scheduleRepaint();
    }

    setState(next);
    setVrrPolicy(props->vrrPolicy.value_or(vrrPolicy()));

    if (!isEnabled() && m_pipeline->needsModeset()) {
        m_gpu->maybeModeset();
    }

    m_renderLoop->setRefreshRate(refreshRate());
    m_renderLoop->scheduleRepaint();

    Q_EMIT changed();
}

void DrmOutput::revertQueuedChanges()
{
    m_pipeline->revertPendingChanges();
}

DrmOutputLayer *DrmOutput::primaryLayer() const
{
    return m_pipeline->primaryLayer();
}

DrmOutputLayer *DrmOutput::cursorLayer() const
{
    return m_pipeline->cursorLayer();
}

bool DrmOutput::setGammaRamp(const std::shared_ptr<ColorTransformation> &transformation)
{
    if (!m_pipeline->activePending() || needsColormanagement()) {
        return false;
    }
    m_pipeline->setGammaRamp(transformation);
    m_pipeline->setCTM(QMatrix3x3());
    if (DrmPipeline::commitPipelines({m_pipeline}, DrmPipeline::CommitMode::Test) == DrmPipeline::Error::None) {
        m_pipeline->applyPendingChanges();
        m_renderLoop->scheduleRepaint();
        return true;
    } else {
        m_pipeline->revertPendingChanges();
        return false;
    }
}

bool DrmOutput::setChannelFactors(const QVector3D &rgb)
{
    if (m_channelFactors == rgb) {
        return true;
    }
    m_channelFactors = rgb;
    if (needsColormanagement()) {
        if (!m_pipeline->activePending()) {
            return false;
        }
        QMatrix3x3 ctm;
        ctm(0, 0) = rgb.x();
        ctm(1, 1) = rgb.y();
        ctm(2, 2) = rgb.z();
        m_pipeline->setCTM(ctm);
        if (DrmPipeline::commitPipelines({m_pipeline}, DrmPipeline::CommitMode::Test) == DrmPipeline::Error::None) {
            m_pipeline->applyPendingChanges();
            m_renderLoop->scheduleRepaint();
            return true;
        } else {
            m_pipeline->revertPendingChanges();
            return false;
        }
    } else {
        m_renderLoop->scheduleRepaint();
        return true;
    }
}

QVector3D DrmOutput::channelFactors() const
{
    return m_channelFactors;
}

bool DrmOutput::needsColormanagement() const
{
    return m_state.wideColorGamut || m_state.highDynamicRange || m_state.iccProfile || m_gpu->isNVidia();
}
}

#include "moc_drm_output.cpp"
