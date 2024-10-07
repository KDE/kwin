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

#include "core/brightnessdevice.h"
#include "core/colortransformation.h"
#include "core/iccprofile.h"
#include "core/outputconfiguration.h"
#include "core/renderbackend.h"
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

static bool s_disableTripleBufferingSet = false;
static const bool s_disableTripleBuffering = qEnvironmentVariableIntValue("KWIN_DRM_DISABLE_TRIPLE_BUFFERING", &s_disableTripleBufferingSet) == 1;

DrmOutput::DrmOutput(const std::shared_ptr<DrmConnector> &conn)
    : m_gpu(conn->gpu())
    , m_pipeline(conn->pipeline())
    , m_connector(conn)
{
    m_pipeline->setOutput(this);
    if (m_gpu->atomicModeSetting() && ((!s_disableTripleBufferingSet && !m_gpu->isNVidia()) || (s_disableTripleBufferingSet && !s_disableTripleBuffering))) {
        m_renderLoop->setMaxPendingFrameCount(2);
    }

    const Edid *edid = m_connector->edid();
    setInformation(Information{
        .name = m_connector->connectorName(),
        .manufacturer = edid->manufacturerString(),
        .model = m_connector->modelName(),
        .serialNumber = edid->serialNumber(),
        .eisaId = edid->eisaId(),
        .physicalSize = m_connector->physicalSize(),
        .edid = *edid,
        .subPixel = m_connector->subpixel(),
        .capabilities = computeCapabilities(),
        .panelOrientation = m_connector->panelOrientation.isValid() ? DrmConnector::toKWinTransform(m_connector->panelOrientation.enumValue()) : OutputTransform::Normal,
        .internal = m_connector->isInternal(),
        .nonDesktop = m_connector->isNonDesktop(),
        .mstPath = m_connector->mstPath(),
        .maxPeakBrightness = edid->desiredMaxLuminance(),
        .maxAverageBrightness = edid->desiredMaxFrameAverageLuminance(),
        .minBrightness = edid->desiredMinLuminance(),
    });
    updateConnectorProperties();

    m_turnOffTimer.setSingleShot(true);
    m_turnOffTimer.setInterval(dimAnimationTime());
    connect(&m_turnOffTimer, &QTimer::timeout, this, [this] {
        if (!setDrmDpmsMode(DpmsMode::Off)) {
            // in case of failure, undo aboutToTurnOff() from setDpmsMode()
            Q_EMIT wakeUp();
        }
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
    const bool tearingDesired = m_desiredPresentationMode == PresentationMode::Async || m_desiredPresentationMode == PresentationMode::AdaptiveAsync;
    if (m_pipeline->gpu()->atomicModeSetting() && tearingDesired && m_pipeline->cursorLayer() && m_pipeline->cursorLayer()->isEnabled()) {
        // The kernel rejects async commits that change anything but the primary plane FB_ID
        // This disables the hardware cursor, so it doesn't interfere with that
        return false;
    }
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
        m_gpu->waitIdle();
    }
    m_pipeline->setActive(active);
    if (DrmPipeline::commitPipelines({m_pipeline}, active ? DrmPipeline::CommitMode::TestAllowModeset : DrmPipeline::CommitMode::CommitModeset) == DrmPipeline::Error::None) {
        m_pipeline->applyPendingChanges();
        updateDpmsMode(mode);
        if (active) {
            m_renderLoop->uninhibit();
            m_renderLoop->scheduleRepaint();
            // re-set KMS color pipeline stuff
            tryKmsColorOffloading();
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
    case OutputTransform::FlipX:
        return PlaneTrans::ReflectX | PlaneTrans::Rotate0;
    case OutputTransform::Rotate90:
        return PlaneTrans::Rotate90;
    case OutputTransform::FlipX90:
        return PlaneTrans::ReflectX | PlaneTrans::Rotate90;
    case OutputTransform::Rotate180:
        return PlaneTrans::Rotate180;
    case OutputTransform::FlipX180:
        return PlaneTrans::ReflectX | PlaneTrans::Rotate180;
    case OutputTransform::Rotate270:
        return PlaneTrans::Rotate270;
    case OutputTransform::FlipX270:
        return PlaneTrans::ReflectX | PlaneTrans::Rotate270;
    default:
        Q_UNREACHABLE();
    }
}

void DrmOutput::updateConnectorProperties()
{
    updateInformation();

    State next = m_state;
    next.modes = getModes();

    if (m_pipeline->crtc()) {
        const auto currentMode = m_pipeline->connector()->findMode(m_pipeline->crtc()->queryCurrentMode());
        if (currentMode != m_pipeline->mode()) {
            // DrmConnector::findCurrentMode might fail
            m_pipeline->setMode(currentMode ? currentMode : m_pipeline->connector()->modes().constFirst());
        }
    }

    next.currentMode = m_pipeline->mode();
    if (!next.currentMode) {
        // some mode needs to be set
        next.currentMode = next.modes.constFirst();
        m_pipeline->setMode(std::static_pointer_cast<DrmConnectorMode>(next.currentMode));
        m_pipeline->applyPendingChanges();
    }
    if (!next.modes.contains(next.currentMode)) {
        next.modes.push_front(next.currentMode);
    }

    setState(next);
}

static const bool s_allowColorspaceIntel = qEnvironmentVariableIntValue("KWIN_DRM_ALLOW_INTEL_COLORSPACE") == 1;
static const bool s_allowColorspaceNVidia = qEnvironmentVariableIntValue("KWIN_DRM_ALLOW_NVIDIA_COLORSPACE") == 1;

Output::Capabilities DrmOutput::computeCapabilities() const
{
    Capabilities capabilities = Capability::Dpms | Capability::IccProfile | Capability::BrightnessControl;
    if (m_connector->overscan.isValid() || m_connector->underscan.isValid()) {
        capabilities |= Capability::Overscan;
    }
    if (m_connector->vrrCapable.isValid() && m_connector->vrrCapable.value()) {
        capabilities |= Capability::Vrr;
    }
    if (m_gpu->asyncPageflipSupported()) {
        capabilities |= Capability::Tearing;
    }
    if (m_connector->broadcastRGB.isValid()) {
        capabilities |= Capability::RgbRange;
    }
    if (m_connector->hdrMetadata.isValid() && m_connector->edid()->supportsPQ()) {
        capabilities |= Capability::HighDynamicRange;
    }
    if (m_connector->colorspace.isValid() && (m_connector->colorspace.hasEnum(DrmConnector::Colorspace::BT2020_RGB) || m_connector->colorspace.hasEnum(DrmConnector::Colorspace::BT2020_YCC)) && m_connector->edid()->supportsBT2020()) {
        bool allowColorspace = true;
        if (m_gpu->isI915()) {
            allowColorspace &= s_allowColorspaceIntel;
        } else if (m_gpu->isNVidia()) {
            allowColorspace &= s_allowColorspaceNVidia;
        }
        if (allowColorspace) {
            capabilities |= Capability::WideColorGamut;
        }
    }
    if (m_connector->isInternal()) {
        // TODO only set this if an orientation sensor is available?
        capabilities |= Capability::AutoRotation;
    }
    return capabilities;
}

void DrmOutput::updateInformation()
{
    // not all changes are currently handled by the rest of KWin
    // so limit the changes to what's verified to work
    const Edid *edid = m_connector->edid();
    Information nextInfo = m_information;
    nextInfo.capabilities = computeCapabilities();
    nextInfo.maxPeakBrightness = edid->desiredMaxLuminance();
    nextInfo.maxAverageBrightness = edid->desiredMaxFrameAverageLuminance();
    nextInfo.minBrightness = edid->desiredMinLuminance();
    setInformation(nextInfo);
}

void DrmOutput::updateDpmsMode(DpmsMode dpmsMode)
{
    State next = m_state;
    next.dpmsMode = dpmsMode;
    setState(next);
}

bool DrmOutput::present(const std::shared_ptr<OutputFrame> &frame)
{
    m_desiredPresentationMode = frame->presentationMode();
    const bool needsModeset = m_gpu->needsModeset();
    bool success;
    if (needsModeset) {
        m_pipeline->setPresentationMode(PresentationMode::VSync);
        m_pipeline->setContentType(DrmConnector::DrmContentType::Graphics);
        success = m_pipeline->maybeModeset(frame);
    } else {
        m_pipeline->setPresentationMode(frame->presentationMode());
        if (m_pipeline->cursorLayer()->isEnabled()) {
            // the cursor plane needs to be disabled before we enable tearing; see DrmOutput::updateCursorLayer
            if (frame->presentationMode() == PresentationMode::AdaptiveAsync) {
                m_pipeline->setPresentationMode(PresentationMode::AdaptiveSync);
            } else if (frame->presentationMode() == PresentationMode::Async) {
                m_pipeline->setPresentationMode(PresentationMode::VSync);
            }
        }
        DrmPipeline::Error err = m_pipeline->present(frame);
        if (err != DrmPipeline::Error::None && frame->presentationMode() == PresentationMode::AdaptiveAsync) {
            // tearing can fail in various circumstances, but vrr shouldn't
            m_pipeline->setPresentationMode(PresentationMode::AdaptiveSync);
            err = m_pipeline->present(frame);
        }
        if (err != DrmPipeline::Error::None && frame->presentationMode() != PresentationMode::VSync) {
            // retry with the most basic presentation mode
            m_pipeline->setPresentationMode(PresentationMode::VSync);
            err = m_pipeline->present(frame);
        }
        success = err == DrmPipeline::Error::None;
    }
    m_renderLoop->setPresentationMode(m_pipeline->presentationMode());
    if (success) {
        Q_EMIT outputChange(frame->damage());
    }
    return success;
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
    const bool bt2020 = props->wideColorGamut.value_or(m_state.wideColorGamut);
    const bool hdr = props->highDynamicRange.value_or(m_state.highDynamicRange);
    m_pipeline->setMode(std::static_pointer_cast<DrmConnectorMode>(mode));
    m_pipeline->setOverscan(props->overscan.value_or(m_pipeline->overscan()));
    m_pipeline->setRgbRange(props->rgbRange.value_or(m_pipeline->rgbRange()));
    m_pipeline->setEnable(props->enabled.value_or(m_pipeline->enabled()));
    m_pipeline->setHighDynamicRange(hdr);
    m_pipeline->setWideColorGamut(bt2020);
    if (bt2020 || hdr || props->colorProfileSource.value_or(m_state.colorProfileSource) != ColorProfileSource::ICC) {
        // ICC profiles don't support HDR (yet)
        m_pipeline->setIccProfile(nullptr);
    } else {
        m_pipeline->setIccProfile(props->iccProfile.value_or(m_state.iccProfile));
    }
    // remove the color pipeline for the atomic test
    // otherwise it could potentially fail
    if (m_gpu->atomicModeSetting()) {
        m_pipeline->setCrtcColorPipeline(ColorPipeline{});
    }
    return true;
}

ColorDescription DrmOutput::createColorDescription(const std::shared_ptr<OutputChangeSet> &props) const
{
    const auto colorSource = props->colorProfileSource.value_or(colorProfileSource());
    const bool hdr = props->highDynamicRange.value_or(m_state.highDynamicRange);
    const bool wcg = props->wideColorGamut.value_or(m_state.wideColorGamut);
    const auto iccProfile = props->iccProfile.value_or(m_state.iccProfile);
    if (colorSource == ColorProfileSource::ICC && !hdr && !wcg && iccProfile) {
        const double minBrightness = iccProfile->minBrightness().value_or(0);
        const double maxBrightness = iccProfile->maxBrightness().value_or(200);
        return ColorDescription(iccProfile->colorimetry(), TransferFunction(TransferFunction::gamma22, minBrightness, maxBrightness), maxBrightness, minBrightness, maxBrightness, maxBrightness);
    }
    const bool supportsHdr = (capabilities() & Capability::HighDynamicRange) && (capabilities() & Capability::WideColorGamut);
    const bool effectiveHdr = hdr && supportsHdr;
    const bool effectiveWcg = wcg && supportsHdr;
    const Colorimetry nativeColorimetry = m_information.edid.colorimetry().value_or(Colorimetry::fromName(NamedColorimetry::BT709));

    const Colorimetry containerColorimetry = effectiveWcg ? Colorimetry::fromName(NamedColorimetry::BT2020) : (colorSource == ColorProfileSource::EDID ? nativeColorimetry : Colorimetry::fromName(NamedColorimetry::BT709));
    const Colorimetry masteringColorimetry = (effectiveWcg || colorSource == ColorProfileSource::EDID) ? nativeColorimetry : Colorimetry::fromName(NamedColorimetry::BT709);
    const Colorimetry sdrColorimetry = effectiveWcg ? Colorimetry::fromName(NamedColorimetry::BT709).interpolateGamutTo(nativeColorimetry, props->sdrGamutWideness.value_or(m_state.sdrGamutWideness)) : Colorimetry::fromName(NamedColorimetry::BT709);
    // TODO the EDID can contain a gamma value, use that when available and colorSource == ColorProfileSource::EDID
    const double maxAverageBrightness = effectiveHdr ? props->maxAverageBrightnessOverride.value_or(m_state.maxAverageBrightnessOverride).value_or(m_connector->edid()->desiredMaxFrameAverageLuminance().value_or(m_state.referenceLuminance)) : 200;
    const double maxPeakBrightness = effectiveHdr ? props->maxPeakBrightnessOverride.value_or(m_state.maxPeakBrightnessOverride).value_or(m_connector->edid()->desiredMaxLuminance().value_or(800)) : 200;
    const double referenceLuminance = effectiveHdr ? props->referenceLuminance.value_or(m_state.referenceLuminance) : maxPeakBrightness;
    const auto transferFunction = TransferFunction{effectiveHdr ? TransferFunction::PerceptualQuantizer : TransferFunction::gamma22}.relativeScaledTo(referenceLuminance);
    // HDR screens are weird, sending them the min. luminance from the EDID does *not* make all of them present the darkest luminance the display can show
    // to work around that, (unless overridden by the user), assume the min. luminance of the transfer function instead
    const double minBrightness = effectiveHdr ? props->minBrightnessOverride.value_or(m_state.minBrightnessOverride).value_or(TransferFunction::defaultMinLuminanceFor(TransferFunction::PerceptualQuantizer)) : transferFunction.minLuminance;

    const double brightnessFactor = !m_brightnessDevice || effectiveHdr ? props->brightness.value_or(m_state.brightness) : 1.0;
    const double effectiveReferenceLuminance = 25 + (referenceLuminance - 25) * brightnessFactor;
    return ColorDescription(containerColorimetry, transferFunction, effectiveReferenceLuminance, minBrightness, maxAverageBrightness, maxPeakBrightness, masteringColorimetry, sdrColorimetry);
}

void DrmOutput::applyQueuedChanges(const std::shared_ptr<OutputChangeSet> &props)
{
    if (!m_connector->isConnected()) {
        return;
    }
    Q_EMIT aboutToChange(props.get());
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
    next.referenceLuminance = props->referenceLuminance.value_or(m_state.referenceLuminance);
    next.wideColorGamut = props->wideColorGamut.value_or(m_state.wideColorGamut);
    next.autoRotatePolicy = props->autoRotationPolicy.value_or(m_state.autoRotatePolicy);
    next.maxPeakBrightnessOverride = props->maxPeakBrightnessOverride.value_or(m_state.maxPeakBrightnessOverride);
    next.maxAverageBrightnessOverride = props->maxAverageBrightnessOverride.value_or(m_state.maxAverageBrightnessOverride);
    next.minBrightnessOverride = props->minBrightnessOverride.value_or(m_state.minBrightnessOverride);
    next.sdrGamutWideness = props->sdrGamutWideness.value_or(m_state.sdrGamutWideness);
    next.iccProfilePath = props->iccProfilePath.value_or(m_state.iccProfilePath);
    next.iccProfile = props->iccProfile.value_or(m_state.iccProfile);
    next.colorDescription = createColorDescription(props);
    next.vrrPolicy = props->vrrPolicy.value_or(m_state.vrrPolicy);
    next.colorProfileSource = props->colorProfileSource.value_or(m_state.colorProfileSource);
    next.brightness = props->brightness.value_or(m_state.brightness);
    next.desiredModeSize = props->desiredModeSize.value_or(m_state.desiredModeSize);
    next.desiredModeRefreshRate = props->desiredModeRefreshRate.value_or(m_state.desiredModeRefreshRate);
    setState(next);

    if (m_brightnessDevice) {
        if (m_state.highDynamicRange) {
            m_brightnessDevice->setBrightness(1);
        } else {
            m_brightnessDevice->setBrightness(m_state.brightness);
        }
    }

    if (!isEnabled() && m_pipeline->needsModeset()) {
        m_gpu->maybeModeset(nullptr);
    }

    m_renderLoop->setRefreshRate(refreshRate());
    m_renderLoop->scheduleRepaint();

    tryKmsColorOffloading();

    Q_EMIT changed();
}

void DrmOutput::setBrightnessDevice(BrightnessDevice *device)
{
    Output::setBrightnessDevice(device);
    if (device) {
        if (m_state.highDynamicRange) {
            device->setBrightness(1);
        } else {
            device->setBrightness(m_state.brightness);
        }
    }
    // reset the brightness factors
    State next = m_state;
    next.colorDescription = createColorDescription(std::make_shared<OutputChangeSet>());
    setState(next);
    tryKmsColorOffloading();
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

bool DrmOutput::setChannelFactors(const QVector3D &rgb)
{
    if (rgb != m_channelFactors) {
        m_channelFactors = rgb;
        tryKmsColorOffloading();
    }
    return true;
}

void DrmOutput::tryKmsColorOffloading()
{
    if (m_state.colorProfileSource == ColorProfileSource::ICC && m_state.iccProfile) {
        // offloading color operations doesn't make sense when we have to apply the icc shader anyways
        m_scanoutColorDescription = colorDescription();
        m_pipeline->setCrtcColorPipeline(ColorPipeline{});
        m_pipeline->applyPendingChanges();
        m_channelFactorsNeedShaderFallback = true;
        return;
    }
    if (!m_pipeline->activePending()) {
        return;
    }
    // TODO this doesn't allow using only a CTM for night light offloading
    // maybe relax correctness in that case and apply night light in non-linear space?
    const QVector3D channelFactors = adaptedChannelFactors();
    const double maxLuminance = colorDescription().maxHdrLuminance().value_or(colorDescription().referenceLuminance());
    const ColorDescription optimal = colorDescription().transferFunction().type == TransferFunction::gamma22 ? colorDescription() : colorDescription().withTransferFunction(TransferFunction(TransferFunction::gamma22, 0, maxLuminance));
    ColorPipeline colorPipeline = ColorPipeline::create(optimal, colorDescription(), RenderingIntent::RelativeColorimetric);
    colorPipeline.addTransferFunction(colorDescription().transferFunction());
    colorPipeline.addMultiplier(channelFactors);
    colorPipeline.addInverseTransferFunction(colorDescription().transferFunction());
    m_pipeline->setCrtcColorPipeline(colorPipeline);
    if (DrmPipeline::commitPipelines({m_pipeline}, DrmPipeline::CommitMode::Test) == DrmPipeline::Error::None) {
        m_pipeline->applyPendingChanges();
        m_scanoutColorDescription = optimal;
        m_channelFactorsNeedShaderFallback = false;
    } else {
        // fall back to using a shadow buffer for doing blending in gamma 2.2 and the channel factors
        m_pipeline->revertPendingChanges();
        m_pipeline->setCrtcColorPipeline(ColorPipeline{});
        m_pipeline->applyPendingChanges();
        m_scanoutColorDescription = colorDescription();
        m_channelFactorsNeedShaderFallback = (channelFactors - QVector3D(1, 1, 1)).lengthSquared() > 0.0001;
    }
}

bool DrmOutput::needsChannelFactorFallback() const
{
    return m_channelFactorsNeedShaderFallback;
}

QVector3D DrmOutput::adaptedChannelFactors() const
{
    const QVector3D ret = ColorDescription::sRGB.toOther(colorDescription(), RenderingIntent::RelativeColorimetric) * m_channelFactors;
    // normalize red to be the original brightness value again
    return ret * m_channelFactors.x() / ret.x();
}

const ColorDescription &DrmOutput::scanoutColorDescription() const
{
    return m_scanoutColorDescription;
}
}

#include "moc_drm_output.cpp"
