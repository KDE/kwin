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
#include "core/session.h"
#include "drm_layer.h"
#include "drm_logging.h"
#include "utils/kernel.h"
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

DrmOutput::DrmOutput(const std::shared_ptr<DrmConnector> &conn, DrmPipeline *pipeline)
    : m_gpu(conn->gpu())
    , m_pipeline(pipeline)
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
        .bitsPerColorRange = BpcRange{
            .min = m_gpu->atomicModeSetting() ? uint32_t(m_connector->maxBpc.minValue()) : 8,
            .max = m_gpu->atomicModeSetting() ? uint32_t(m_connector->maxBpc.maxValue()) : 8,
        },
        .minVrrRefreshRateHz = edid->minVrrRefreshRateHz(),
    });
    updateConnectorProperties();
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

bool DrmOutput::shouldDisableNonPrimaryPlanes() const
{
    // The kernel rejects async commits that change anything but the primary plane FB_ID
    // This disables the hardware cursor, so it doesn't interfere with that
    return m_desiredPresentationMode == PresentationMode::Async || m_desiredPresentationMode == PresentationMode::AdaptiveAsync;
}

bool DrmOutput::presentAsync(OutputLayer *layer, std::optional<std::chrono::nanoseconds> allowedVrrDelay)
{
    if (!m_pipeline) {
        // this can happen when the output gets hot-unplugged
        // FIXME fix output lifetimes so that this doesn't happen anymore...
        return false;
    }
    if (m_pipeline->gpu()->atomicModeSetting() && shouldDisableNonPrimaryPlanes() && layer->isEnabled()) {
        return false;
    }
    return m_pipeline->presentAsync(layer, allowedVrrDelay);
}

QList<std::shared_ptr<OutputMode>> DrmOutput::getModes(const State &state) const
{
    const auto drmModes = m_pipeline->connector()->modes();

    QList<std::shared_ptr<OutputMode>> ret;
    ret.reserve(drmModes.count());
    for (const auto &drmMode : drmModes) {
        ret.append(drmMode);
    }
    for (const auto &custom : state.customModes) {
        ret.append(m_pipeline->connector()->generateMode(custom.size, custom.refreshRate / 1000.0f, custom.flags | OutputMode::Flag::Custom));
    }
    return ret;
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
    next.modes = getModes(next);
    if (!next.currentMode) {
        // some mode needs to be set
        next.currentMode = next.modes.constFirst();
    }
    if (!next.modes.contains(next.currentMode)) {
        next.currentMode->setRemoved();
        next.modes.push_front(next.currentMode);
    }
    setState(next);
}

static const bool s_allowColorspaceIntel = qEnvironmentVariableIntValue("KWIN_DRM_ALLOW_INTEL_COLORSPACE") == 1;
static const bool s_allowColorspaceNVidia = qEnvironmentVariableIntValue("KWIN_DRM_ALLOW_NVIDIA_COLORSPACE") == 1;

BackendOutput::Capabilities DrmOutput::computeCapabilities() const
{
    Capabilities capabilities = Capability::Dpms | Capability::IccProfile;
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
    if (m_connector->colorspace.isValid() && (m_connector->colorspace.hasEnum(DrmConnector::Colorspace::BT2020_RGB) || m_connector->colorspace.hasEnum(DrmConnector::Colorspace::BT2020_YCC)) && m_connector->edid()->supportsBT2020()) {
        bool allowColorspace = true;
        if (m_gpu->isI915()) {
            allowColorspace &= s_allowColorspaceIntel || linuxKernelVersion() >= Version(6, 11);
        } else if (m_gpu->isNVidia()) {
            allowColorspace &= s_allowColorspaceNVidia || m_gpu->nvidiaDriverVersion() >= Version(565, 57, 1);
        }
        if (allowColorspace) {
            capabilities |= Capability::WideColorGamut;
        }
    }
    if (m_connector->hdrMetadata.isValid() && m_connector->edid()->supportsPQ() && (capabilities & Capability::WideColorGamut)) {
        capabilities |= Capability::HighDynamicRange;
    }
    if (m_connector->isInternal() && m_autoRotateAvailable) {
        capabilities |= Capability::AutoRotation;
    }
    if (m_state.highDynamicRange || m_state.brightnessDevice || m_state.allowSdrSoftwareBrightness) {
        capabilities |= Capability::BrightnessControl;
    }
    if (m_connector->edid()->isValid() && m_connector->edid()->defaultColorimetry().has_value()) {
        capabilities |= Capability::BuiltInColorProfile;
    }
    if (m_state.detectedDdcCi) {
        capabilities |= Capability::DdcCi;
    }
    if (m_connector->maxBpc.isValid()) {
        capabilities |= Capability::MaxBitsPerColor;
    }
    if (m_state.brightnessDevice && isInternal()) {
        capabilities |= Capability::Edr;
    }
    if (m_gpu->sharpnessSupported()) {
        capabilities |= Capability::SharpnessControl;
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
    // TODO narrow that down by parsing the EDID and checking what the display supports
    nextInfo.bitsPerColorRange = BpcRange{
        .min = m_gpu->atomicModeSetting() ? uint32_t(m_connector->maxBpc.minValue()) : 8,
        .max = m_gpu->atomicModeSetting() ? uint32_t(m_connector->maxBpc.maxValue()) : 8,
    };
    setInformation(nextInfo);
}

bool DrmOutput::testPresentation(const std::shared_ptr<OutputFrame> &frame)
{
    m_desiredPresentationMode = frame->presentationMode();
    const auto layers = m_pipeline->layers();
    const bool nonPrimaryEnabled = std::ranges::any_of(layers, [](OutputLayer *layer) {
        return layer->isEnabled() && layer->type() != OutputLayerType::Primary;
    });
    if (m_gpu->needsModeset()) {
        // modesets should be done with only the primary plane
        // as additional planes may mean we can't power all outputs
        if (nonPrimaryEnabled) {
            return false;
        }
        // the atomic test for the modeset has already been done before
        // so testing again isn't super useful
        return true;
    }
    m_pipeline->setPresentationMode(frame->presentationMode());
    if (nonPrimaryEnabled) {
        // the cursor plane needs to be disabled before we enable tearing; see DrmOutput::presentAsync
        if (frame->presentationMode() == PresentationMode::AdaptiveAsync) {
            m_pipeline->setPresentationMode(PresentationMode::AdaptiveSync);
        } else if (frame->presentationMode() == PresentationMode::Async) {
            m_pipeline->setPresentationMode(PresentationMode::VSync);
        }
    }
    DrmPipeline::Error err = m_pipeline->testPresent(frame);
    if (err != DrmPipeline::Error::None && frame->presentationMode() == PresentationMode::AdaptiveAsync) {
        // tearing can fail in various circumstances, but vrr shouldn't
        m_pipeline->setPresentationMode(PresentationMode::AdaptiveSync);
        err = m_pipeline->testPresent(frame);
    }
    if (err != DrmPipeline::Error::None && frame->presentationMode() != PresentationMode::VSync) {
        // retry with the most basic presentation mode
        m_pipeline->setPresentationMode(PresentationMode::VSync);
        err = m_pipeline->testPresent(frame);
    }
    return err == DrmPipeline::Error::None;
}

bool DrmOutput::present(const QList<OutputLayer *> &layersToUpdate, const std::shared_ptr<OutputFrame> &frame)
{
    m_desiredPresentationMode = frame->presentationMode();
    const bool needsModeset = m_gpu->needsModeset();
    bool success;
    if (needsModeset) {
        m_pipeline->setPresentationMode(PresentationMode::VSync);
        m_pipeline->setContentType(DrmConnector::DrmContentType::Graphics);
        m_pipeline->maybeModeset(frame);
        success = true;
    } else {
        // the presentation mode of the pipeline is already set in testPresentation
        success = m_pipeline->present(layersToUpdate, frame) == DrmPipeline::Error::None;
    }
    m_renderLoop->setPresentationMode(m_pipeline->presentationMode());
    if (!success) {
        return false;
    }
    if (frame->brightness() != m_state.currentBrightness || (frame->artificialHdrHeadroom() && frame->artificialHdrHeadroom() != m_state.artificialHdrHeadroom)) {
        updateBrightness(frame->brightness().value_or(m_state.currentBrightness.value_or(m_state.brightnessSetting)), frame->artificialHdrHeadroom().value_or(m_state.artificialHdrHeadroom));
    }
    return true;
}

void DrmOutput::repairPresentation()
{
    // read back drm properties, most likely our info is out of date somehow
    // or we need a modeset
    QTimer::singleShot(0, m_gpu->platform(), &DrmBackend::updateOutputs);
}

bool DrmOutput::overlayLayersLikelyBroken() const
{
    return m_gpu->isNVidia();
}

DrmConnector *DrmOutput::connector() const
{
    return m_connector.get();
}

DrmPipeline *DrmOutput::pipeline() const
{
    return m_pipeline;
}

std::optional<uint32_t> DrmOutput::decideAutomaticBpcLimit() const
{
    static bool preferreedColorDepthIsSet = false;
    static const int preferred = qEnvironmentVariableIntValue("KWIN_DRM_PREFER_COLOR_DEPTH", &preferreedColorDepthIsSet);
    if (preferreedColorDepthIsSet) {
        return preferred / 3;
    }
    if (!m_connector->mstPath().isEmpty()) {
        // >8bpc is often broken with docks
        return 8;
    }
    return std::nullopt;
}

bool DrmOutput::queueChanges(const std::shared_ptr<OutputChangeSet> &props)
{
    const auto mode = props->mode.value_or(currentMode()).lock();
    if (!mode) {
        return false;
    }
    const bool bt2020 = props->wideColorGamut.value_or(m_state.wideColorGamut) && (capabilities() & Capability::WideColorGamut);
    const bool hdr = props->highDynamicRange.value_or(m_state.highDynamicRange) && (capabilities() & Capability::HighDynamicRange);
    m_pipeline->setMode(std::static_pointer_cast<DrmConnectorMode>(mode));
    m_pipeline->setOverscan(props->overscan.value_or(m_pipeline->overscan()));
    m_pipeline->setRgbRange(props->rgbRange.value_or(m_pipeline->rgbRange()));
    m_pipeline->setEnable(props->enabled.value_or(m_pipeline->enabled()));
    m_pipeline->setActive(m_pipeline->enabled() && props->dpmsMode.value_or(m_state.dpmsMode) != DpmsMode::Off);
    m_pipeline->setHighDynamicRange(hdr);
    m_pipeline->setWideColorGamut(bt2020);

    if (uint32_t bpcSetting = props->maxBitsPerColor.value_or(maxBitsPerColor())) {
        m_pipeline->setMaxBpc(bpcSetting);
    } else {
        const auto tradeoff = props->colorPowerTradeoff.value_or(m_state.colorPowerTradeoff);
        m_pipeline->setMaxBpc(decideAutomaticBpcLimit().value_or(tradeoff == ColorPowerTradeoff::PreferAccuracy ? 16 : 10));
    }

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

static QVector3D adaptChannelFactors(const std::shared_ptr<ColorDescription> &originalColor, const QVector3D &sRGBchannelFactors)
{
    QVector3D adaptedChannelFactors = ColorDescription::sRGB->containerColorimetry().relativeColorimetricTo(originalColor->containerColorimetry()) * sRGBchannelFactors;
    // ensure none of the values reach zero, otherwise the white point might end up on or outside
    // the edges of the gamut, which leads to terrible glitches
    adaptedChannelFactors.setX(std::max(adaptedChannelFactors.x(), 0.0001f));
    adaptedChannelFactors.setY(std::max(adaptedChannelFactors.y(), 0.0001f));
    adaptedChannelFactors.setZ(std::max(adaptedChannelFactors.z(), 0.0001f));
    return adaptedChannelFactors;
}

static std::shared_ptr<ColorDescription> applyNightLight(const std::shared_ptr<ColorDescription> &originalColor, const QVector3D &sRGBchannelFactors)
{
    const QVector3D adapted = adaptChannelFactors(originalColor, sRGBchannelFactors);
    // calculate the white point
    // this includes the maximum brightness we can do without clipping any color channel as well
    const xyY newWhite = XYZ::fromVector(originalColor->containerColorimetry().toXYZ() * adapted).toxyY();
    return originalColor->withWhitepoint(newWhite)->dimmed(newWhite.Y);
}

std::shared_ptr<ColorDescription> DrmOutput::createColorDescription(const State &next) const
{
    const bool effectiveHdr = next.highDynamicRange && (capabilities() & Capability::HighDynamicRange);
    const bool effectiveWcg = next.wideColorGamut && (capabilities() & Capability::WideColorGamut);
    const double brightness = next.currentBrightness.value_or(next.brightnessSetting);
    double maxPossibleArtificialHeadroom = 1.0;
    if (next.brightnessDevice && isInternal() && next.edrPolicy == EdrPolicy::Always) {
        maxPossibleArtificialHeadroom = std::min(1.0 / next.currentBrightness.value_or(next.brightnessSetting), 3.0);
    }

    if (next.colorProfileSource == ColorProfileSource::ICC && !effectiveHdr && !effectiveWcg && next.iccProfile) {
        const double maxFALL = next.iccProfile->maxFALL().value_or(200);
        const double minBrightness = next.iccProfile->relativeBlackPoint().value_or(0) * maxFALL;
        const auto sdrColor = Colorimetry::BT709.interpolateGamutTo(next.iccProfile->colorimetry(), next.sdrGamutWideness);
        const double brightnessFactor = (!next.brightnessDevice && next.allowSdrSoftwareBrightness) ? brightness : 1.0;
        const double effectiveReferenceLuminance = 5 + (maxFALL - 5) * brightnessFactor;
        return std::make_shared<ColorDescription>(ColorDescription{
            next.iccProfile->colorimetry(),
            TransferFunction(TransferFunction::gamma22, minBrightness, maxFALL * next.artificialHdrHeadroom),
            effectiveReferenceLuminance,
            minBrightness * next.artificialHdrHeadroom,
            maxFALL * maxPossibleArtificialHeadroom,
            maxFALL * maxPossibleArtificialHeadroom,
            next.iccProfile->colorimetry(),
            sdrColor,
        });
    }

    const Colorimetry nativeColorimetry = m_information.edid.nativeColorimetry().value_or(Colorimetry::BT709);
    const Colorimetry containerColorimetry = effectiveWcg ? Colorimetry::BT2020 : (next.colorProfileSource == ColorProfileSource::EDID ? nativeColorimetry : Colorimetry::BT709);
    const Colorimetry masteringColorimetry = (effectiveWcg || next.colorProfileSource == ColorProfileSource::EDID) ? nativeColorimetry : Colorimetry::BT709;
    const Colorimetry sdrColorimetry = (effectiveWcg || next.colorProfileSource == ColorProfileSource::EDID) ? Colorimetry::BT709.interpolateGamutTo(nativeColorimetry, next.sdrGamutWideness) : Colorimetry::BT709;
    // TODO the EDID can contain a gamma value, use that when available and colorSource == ColorProfileSource::EDID
    const double maxAverageBrightness = effectiveHdr ? next.maxAverageBrightnessOverride.value_or(m_connector->edid()->desiredMaxFrameAverageLuminance().value_or(next.referenceLuminance)) : 200;
    const double maxPeakBrightness = effectiveHdr ? next.maxPeakBrightnessOverride.value_or(m_connector->edid()->desiredMaxLuminance().value_or(800)) : 200 * maxPossibleArtificialHeadroom;
    const double referenceLuminance = effectiveHdr ? next.referenceLuminance : 200;
    // the min luminance the Wayland protocol defines for SDR is unrealistically high for most modern displays
    // normally that doesn't really matter, but with night light it can lead to increased black levels,
    // which are really noticeable when they're tinted red
    const double minSdrLuminance = 0.01;
    const auto transferFunction = effectiveHdr ? TransferFunction{TransferFunction::PerceptualQuantizer} : TransferFunction{TransferFunction::gamma22, minSdrLuminance * next.artificialHdrHeadroom, referenceLuminance * next.artificialHdrHeadroom};
    // HDR screens are weird, sending them the min. luminance from the EDID does *not* make all of them present the darkest luminance the display can show
    // to work around that, (unless overridden by the user), assume the min. luminance of the transfer function instead
    const double minBrightness = effectiveHdr ? next.minBrightnessOverride.value_or(transferFunction.minLuminance) : transferFunction.minLuminance;

    const double brightnessFactor = (!next.brightnessDevice && next.allowSdrSoftwareBrightness) || effectiveHdr ? brightness : 1.0;
    const double effectiveReferenceLuminance = 5 + (referenceLuminance - 5) * brightnessFactor;
    return std::make_shared<ColorDescription>(ColorDescription{
        containerColorimetry,
        transferFunction,
        effectiveReferenceLuminance,
        minBrightness,
        maxAverageBrightness,
        maxPeakBrightness,
        masteringColorimetry,
        sdrColorimetry,
    });
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
    next.scaleSetting = props->scaleSetting.value_or(m_state.scaleSetting);
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
    next.vrrPolicy = props->vrrPolicy.value_or(m_state.vrrPolicy);
    next.colorProfileSource = props->colorProfileSource.value_or(m_state.colorProfileSource);
    next.brightnessSetting = props->brightness.value_or(m_state.brightnessSetting);
    next.currentBrightness = props->currentBrightness.has_value() ? props->currentBrightness : m_state.currentBrightness;
    next.desiredModeSize = props->desiredModeSize.value_or(m_state.desiredModeSize);
    next.desiredModeRefreshRate = props->desiredModeRefreshRate.value_or(m_state.desiredModeRefreshRate);
    next.allowSdrSoftwareBrightness = props->allowSdrSoftwareBrightness.value_or(m_state.allowSdrSoftwareBrightness);
    next.colorPowerTradeoff = props->colorPowerTradeoff.value_or(m_state.colorPowerTradeoff);
    next.dimming = props->dimming.value_or(m_state.dimming);
    next.brightnessDevice = props->brightnessDevice.value_or(m_state.brightnessDevice);
    next.uuid = props->uuid.value_or(m_state.uuid);
    next.replicationSource = props->replicationSource.value_or(m_state.replicationSource);
    next.detectedDdcCi = props->detectedDdcCi.value_or(m_state.detectedDdcCi);
    next.allowDdcCi = props->allowDdcCi.value_or(m_state.allowDdcCi);
    if (next.allowSdrSoftwareBrightness != m_state.allowSdrSoftwareBrightness) {
        // make sure that we set the brightness again next frame
        next.currentBrightness.reset();
    }
    next.maxBitsPerColor = props->maxBitsPerColor.value_or(m_state.maxBitsPerColor);
    next.automaticMaxBitsPerColorLimit = decideAutomaticBpcLimit();
    next.edrPolicy = props->edrPolicy.value_or(m_state.edrPolicy);
    next.dpmsMode = props->dpmsMode.value_or(m_state.dpmsMode);
    if (props->customModes.has_value()) {
        next.customModes = *props->customModes;
        next.modes = getModes(next);
    }
    next.originalColorDescription = createColorDescription(next);
    next.colorDescription = applyNightLight(next.originalColorDescription, m_sRgbChannelFactors);
    next.sharpnessSetting = props->sharpness.value_or(m_state.sharpnessSetting);
    next.priority = props->priority.value_or(m_state.priority);
    next.deviceOffset = props->deviceOffset.value_or(m_state.deviceOffset);
    tryKmsColorOffloading(next);
    maybeScheduleRepaints(next);
    if (next.dpmsMode != m_state.dpmsMode) {
        if (next.dpmsMode == DpmsMode::On) {
            m_renderLoop->uninhibit();
        } else {
            m_renderLoop->inhibit();
        }
    }
    setState(next);

    // allowSdrSoftwareBrightness, the brightness device or detectedDdcCi might change our capabilities
    Information newInfo = m_information;
    newInfo.capabilities = computeCapabilities();
    setInformation(newInfo);

    if (!m_pipeline->activePending() && m_gpu->needsModeset()) {
        // If the output is active, state changes end up being committed when presenting.
        // However, if it's off, that needs to be done explicitly
        m_gpu->maybeModeset(nullptr, nullptr);
    }

    m_renderLoop->setRefreshRate(refreshRate());

    if (m_state.brightnessDevice && m_state.highDynamicRange && isInternal()) {
        // This is usually not necessary with external monitors, as they default to 100% in HDR mode on their own,
        // and is known to even cause problems with some buggy ones.
        // This is however needed for laptop displays to have the desired luminance levels
        m_state.brightnessDevice->setBrightness(1.0);
    }

    Q_EMIT changed();
}

void DrmOutput::unsetBrightnessDevice()
{
    State next = m_state;
    next.brightnessDevice = nullptr;
    setState(next);
    updateInformation();
}

void DrmOutput::updateBrightness(double newBrightness, double newArtificialHdrHeadroom)
{
    if (m_state.brightnessDevice && !m_state.highDynamicRange) {
        constexpr double minLuminance = 0.04;
        const double effectiveBrightness = (minLuminance + newBrightness) * newArtificialHdrHeadroom - minLuminance;
        m_state.brightnessDevice->setBrightness(effectiveBrightness);
    }
    State next = m_state;
    next.currentBrightness = newBrightness;
    next.artificialHdrHeadroom = newArtificialHdrHeadroom;
    next.originalColorDescription = createColorDescription(next);
    next.colorDescription = applyNightLight(next.originalColorDescription, m_sRgbChannelFactors);
    tryKmsColorOffloading(next);
    setState(next);
}

void DrmOutput::revertQueuedChanges()
{
    m_pipeline->revertPendingChanges();
}

bool DrmOutput::setChannelFactors(const QVector3D &rgb)
{
    if (rgb != m_sRgbChannelFactors) {
        m_sRgbChannelFactors = rgb;
        State next = m_state;
        next.colorDescription = applyNightLight(next.originalColorDescription, m_sRgbChannelFactors);
        tryKmsColorOffloading(next);
        setState(next);
    }
    return true;
}

void DrmOutput::tryKmsColorOffloading(State &next)
{
    const auto repaints = qScopeGuard([this, &next]() {
        maybeScheduleRepaints(next);
    });

    constexpr TransferFunction::Type blendingSpace = TransferFunction::gamma22;
    const double maxLuminance = next.colorDescription->maxHdrLuminance().value_or(next.colorDescription->referenceLuminance());
    if (next.colorDescription->transferFunction().type == blendingSpace) {
        next.blendingColor = next.colorDescription;
    } else {
        next.blendingColor = next.colorDescription->withTransferFunction(TransferFunction(blendingSpace, 0, maxLuminance));
    }

    // we can't use the original color description without modifications
    // as that would un-do any brightness adjustments we did for night light
    // note that we also can't use ColorDescription::dimmed, as we must avoid clipping to this luminance!
    const auto encoding = next.originalColorDescription->withReference(next.colorDescription->referenceLuminance());

    // absolute colorimetric to preserve the whitepoint adjustments made during compositing
    ColorPipeline colorPipeline = ColorPipeline::create(next.blendingColor, encoding, RenderingIntent::AbsoluteColorimetricNoAdaptation);

    const bool hdr = next.highDynamicRange && (capabilities() & Capability::HighDynamicRange);
    const bool wcg = next.wideColorGamut && (capabilities() & Capability::WideColorGamut);
    const bool usesICC = next.colorProfileSource == ColorProfileSource::ICC && next.iccProfile && !hdr && !wcg;
    if (next.colorPowerTradeoff == ColorPowerTradeoff::PreferAccuracy) {
        next.layerBlendingColor = encoding;
        m_pipeline->setCrtcColorPipeline(ColorPipeline{});
        m_pipeline->applyPendingChanges();
        m_needsShadowBuffer = usesICC
            || next.colorDescription->transferFunction().type != blendingSpace
            || !colorPipeline.isIdentity();
        return;
    }
    if (!m_pipeline->activePending() || m_pipeline->layers().empty()) {
        return;
    }
    if (usesICC) {
        colorPipeline.addTransferFunction(encoding->transferFunction(), ColorspaceType::LinearRGB);
        colorPipeline.addMultiplier(1.0 / encoding->transferFunction().maxLuminance);
        const auto calibration = encoding->containerColorimetry().fromXYZ() * next.iccProfile->mhc2Matrix() * encoding->containerColorimetry().toXYZ();
        colorPipeline.addMatrix(calibration, colorPipeline.currentOutputRange(), ColorspaceType::LinearRGB);
        colorPipeline.add1DLUT(next.iccProfile->inverseTransferFunction(), ColorspaceType::NonLinearRGB);
        if (next.iccProfile->vcgt()) {
            colorPipeline.add1DLUT(next.iccProfile->vcgt(), ColorspaceType::NonLinearRGB);
        }
    }
    m_pipeline->setCrtcColorPipeline(colorPipeline);
    if (DrmPipeline::commitPipelines({m_pipeline}, DrmPipeline::CommitMode::Test) == DrmPipeline::Error::None) {
        m_pipeline->applyPendingChanges();
        next.layerBlendingColor = next.blendingColor;
        m_needsShadowBuffer = false;
        return;
    }
    if (next.colorDescription->transferFunction().type == blendingSpace && !usesICC) {
        // Allow falling back to applying night light in non-linear space.
        // This isn't technically correct, but the difference is quite small and not worth
        // losing a lot of performance and battery life over
        ColorPipeline simplerPipeline(ValueRange{0, 1}, ColorspaceType::NonLinearRGB);
        simplerPipeline.addMatrix(next.blendingColor->toOther(*encoding, RenderingIntent::AbsoluteColorimetricNoAdaptation), colorPipeline.currentOutputRange(), ColorspaceType::NonLinearRGB);
        m_pipeline->setCrtcColorPipeline(simplerPipeline);
        if (DrmPipeline::commitPipelines({m_pipeline}, DrmPipeline::CommitMode::Test) == DrmPipeline::Error::None) {
            m_pipeline->applyPendingChanges();
            next.layerBlendingColor = next.blendingColor;
            m_needsShadowBuffer = false;
            return;
        }
    }
    // fall back to using a shadow buffer for doing blending in gamma 2.2 and/or night light
    m_pipeline->setCrtcColorPipeline(ColorPipeline{});
    m_pipeline->applyPendingChanges();
    next.layerBlendingColor = encoding;
    m_needsShadowBuffer = usesICC
        || next.colorDescription->transferFunction().type != blendingSpace
        || !colorPipeline.isIdentity();
}

void DrmOutput::maybeScheduleRepaints(const State &next)
{
    // TODO move the output layers to BackendOutput, and have it take care of this when updating State
    if (next.blendingColor != m_state.blendingColor || next.layerBlendingColor != m_state.layerBlendingColor) {
        const auto layers = m_pipeline->layers();
        for (const auto &layer : layers) {
            layer->addDeviceRepaint(infiniteRegion());
        }
    }
}

bool DrmOutput::needsShadowBuffer() const
{
    return m_needsShadowBuffer;
}

void DrmOutput::removePipeline()
{
    m_pipeline = nullptr;
}

void DrmOutput::setAutoRotateAvailable(bool isAvailable)
{
    m_autoRotateAvailable = isAvailable;
    Information next = m_information;
    next.capabilities = computeCapabilities();
    setInformation(next);
}
}

#include "moc_drm_output.cpp"
