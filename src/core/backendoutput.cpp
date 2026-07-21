/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "backendoutput.h"
#include "brightnessdevice.h"
#include "iccprofile.h"
#include "outputconfiguration.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <QJsonArray>

namespace KWin
{

AutoBrightnessCurve::AutoBrightnessCurve()
    : m_luxToBrightness({{0, 1}})
{
}

AutoBrightnessCurve::AutoBrightnessCurve(Qt::Initialization)
{
}

double AutoBrightnessCurve::sample(double lux) const
{
    Q_ASSERT(!m_luxToBrightness.empty());
    if (const auto higher = m_luxToBrightness.lower_bound(lux); higher == m_luxToBrightness.begin()) {
        return higher->second;
    } else if (const auto lower = std::prev(higher); higher == m_luxToBrightness.end()) {
        return lower->second;
    } else {
        return std::lerp(lower->second, higher->second, (lux - lower->first) / (higher->first - lower->first));
    }
}

void AutoBrightnessCurve::adjust(double brightness, double lux)
{
    // find control point to replace (or insertion point if none match)
    auto range = m_luxToBrightness.equal_range(lux);
    // keep the curve non-decreasing by erasing brighter control points at lower light levels...
    while (range.first != m_luxToBrightness.begin() && std::prev(range.first)->second >= brightness) {
        --range.first;
    }
    // ...and dimmer control points at higher light levels
    while (range.second != m_luxToBrightness.end() && range.second->second <= brightness) {
        ++range.second;
    }
    // avoid proliferation of control points by retaining only the minimum
    // and maximum lightness values that share the specified brightness value
    if (range.first != range.second && range.first->second == brightness && range.first->first < lux) {
        ++range.first;
    }
    if (range.second != range.first) {
        if (const auto last = std::prev(range.second); last->second == brightness && last->first > lux) {
            --range.second;
        }
    }
    const auto pos = m_luxToBrightness.erase(range.first, range.second);
    // insert new point only if it would not fall between two points with the same brightness
    if (pos == m_luxToBrightness.begin() || pos == m_luxToBrightness.end() || std::prev(pos)->second != pos->second) {
        m_luxToBrightness.emplace_hint(pos, lux, brightness);
    }
}

QJsonArray AutoBrightnessCurve::toArray() const
{
    QJsonArray ret;
    for (const auto &[lux, brightness] : m_luxToBrightness) {
        ret.push_back(QJsonArray{lux, brightness});
    }
    return ret;
}

std::optional<AutoBrightnessCurve> AutoBrightnessCurve::fromArray(const QJsonArray &array)
{
    if (array.empty()) {
        return std::nullopt;
    }
    AutoBrightnessCurve ret(Qt::Uninitialized);
    qsizetype index = 0;
    for (const auto &value : array) {
        double lux;
        double brightness;
        if (value.isArray()) {
            const auto &point = value.toArray();
            if (point.size() != 2 || !point[0].isDouble() || !point[1].isDouble()) {
                return std::nullopt;
            }
            lux = value[0].toDouble();
            brightness = value[1].toDouble();
        } else if (value.isDouble()) { // import from obsolete format
            lux = value.toDouble();
            brightness = double(index) / (array.size() - 1);
        } else {
            return std::nullopt;
        }
        ret.adjust(brightness, lux);
        index++;
    }

    return ret;
}

BackendOutput::BackendOutput(DrmDevice *scanoutDevice)
    : m_renderLoop(std::make_unique<RenderLoop>(this))
    , m_scanoutDevice(scanoutDevice)
{
}

BackendOutput::~BackendOutput()
{
}

void BackendOutput::ref()
{
    m_refCount++;
}

void BackendOutput::unref()
{
    Q_ASSERT(m_refCount > 0);
    m_refCount--;
    if (m_refCount == 0) {
        delete this;
    }
}

QString BackendOutput::name() const
{
    return m_information.name;
}

QString BackendOutput::uuid() const
{
    return m_state.uuid;
}

OutputTransform BackendOutput::transform() const
{
    return m_state.transform;
}

OutputTransform BackendOutput::manualTransform() const
{
    return m_state.manualTransform;
}

QString BackendOutput::eisaId() const
{
    return m_information.eisaId;
}

QString BackendOutput::manufacturer() const
{
    return m_information.manufacturer;
}

QString BackendOutput::model() const
{
    return m_information.model;
}

QString BackendOutput::serialNumber() const
{
    return m_information.serialNumber;
}

bool BackendOutput::isInternal() const
{
    return m_information.internal;
}

BackendOutput::Capabilities BackendOutput::capabilities() const
{
    return m_information.capabilities;
}

qreal BackendOutput::scale() const
{
    return m_state.scale;
}

QPoint BackendOutput::position() const
{
    return m_state.position;
}

QSize BackendOutput::physicalSize() const
{
    return m_information.physicalSize;
}

uint32_t BackendOutput::refreshRate() const
{
    return m_state.currentMode ? m_state.currentMode->refreshRate() : 0;
}

QSize BackendOutput::modeSize() const
{
    return m_state.currentMode ? m_state.currentMode->size() : QSize();
}

QSize BackendOutput::pixelSize() const
{
    return orientateSize(modeSize());
}

const Edid &BackendOutput::edid() const
{
    return m_information.edid;
}

QList<std::shared_ptr<OutputMode>> BackendOutput::modes() const
{
    return m_state.modes;
}

std::shared_ptr<OutputMode> BackendOutput::currentMode() const
{
    return m_state.currentMode;
}

OutputModeline BackendOutput::desiredMode() const
{
    return m_state.desiredMode;
}

BackendOutput::SubPixel BackendOutput::subPixel() const
{
    return m_information.subPixel;
}

void BackendOutput::applyChanges(const OutputConfiguration &config)
{
    auto props = config.constChangeSet(this);
    if (!props) {
        return;
    }
    Q_EMIT aboutToChange(props.get());

    State next = m_state;
    next.enabled = props->enabled.value_or(m_state.enabled);
    next.transform = props->transform.value_or(m_state.transform);
    next.position = props->pos.value_or(m_state.position);
    next.scale = props->scale.value_or(m_state.scale);
    next.scaleSetting = props->scaleSetting.value_or(m_state.scaleSetting);
    next.rgbRange = props->rgbRange.value_or(m_state.rgbRange);
    next.autoRotatePolicy = props->autoRotationPolicy.value_or(m_state.autoRotatePolicy);
    next.iccProfilePath = props->iccProfilePath.value_or(m_state.iccProfilePath);
    if (props->iccProfilePath) {
        next.iccProfile = IccProfile::load(*props->iccProfilePath).value_or(nullptr);
    }
    next.vrrPolicy = props->vrrPolicy.value_or(m_state.vrrPolicy);
    next.desiredMode = props->desiredMode.value_or(m_state.desiredMode);
    next.uuid = props->uuid.value_or(m_state.uuid);
    next.replicationSource = props->replicationSource.value_or(m_state.replicationSource);
    next.priority = props->priority.value_or(m_state.priority);
    next.deviceOffset = props->deviceOffset.value_or(m_state.deviceOffset);

    setState(next);

    Q_EMIT changed();
}

bool BackendOutput::canResize() const
{
    return false;
}

void BackendOutput::resize(const QSize &size)
{
    Q_ASSERT(false);
}

bool BackendOutput::isEnabled() const
{
    return m_state.enabled;
}

QString BackendOutput::description() const
{
    return manufacturer() + ' ' + model();
}

void BackendOutput::setInformation(const Information &information)
{
    const auto oldInfo = m_information;
    m_information = information;
    if (oldInfo.capabilities != information.capabilities) {
        Q_EMIT capabilitiesChanged();
    }
}

void BackendOutput::setState(const State &state)
{
    Q_ASSERT(!state.modes.isEmpty());

    const State oldState = m_state;
    m_state = state;

    if (oldState.position != state.position) {
        Q_EMIT positionChanged();
    }
    if (oldState.scaleSetting != state.scaleSetting) {
        Q_EMIT scaleSettingChanged();
    }
    if (oldState.scale != state.scale) {
        Q_EMIT scaleChanged();
    }
    if (oldState.modes != state.modes) {
        Q_EMIT modesChanged();
    }
    if (oldState.currentMode != state.currentMode) {
        Q_EMIT currentModeChanged();
    }
    if (oldState.transform != state.transform) {
        Q_EMIT transformChanged();
    }
    if (oldState.overscan != state.overscan) {
        Q_EMIT overscanChanged();
    }
    if (oldState.dpmsMode != state.dpmsMode) {
        Q_EMIT dpmsModeChanged();
    }
    if (oldState.rgbRange != state.rgbRange) {
        Q_EMIT rgbRangeChanged();
    }
    if (oldState.highDynamicRange != state.highDynamicRange) {
        Q_EMIT highDynamicRangeChanged();
    }
    if (oldState.referenceLuminance != state.referenceLuminance) {
        Q_EMIT referenceLuminanceChanged();
    }
    if (oldState.wideColorGamut != state.wideColorGamut) {
        Q_EMIT wideColorGamutChanged();
    }
    if (oldState.autoRotatePolicy != state.autoRotatePolicy) {
        Q_EMIT autoRotationPolicyChanged();
    }
    if (oldState.iccProfile != state.iccProfile) {
        Q_EMIT iccProfileChanged();
    }
    if (oldState.iccProfilePath != state.iccProfilePath) {
        Q_EMIT iccProfilePathChanged();
    }
    if (oldState.maxPeakBrightnessOverride != state.maxPeakBrightnessOverride
        || oldState.maxAverageBrightnessOverride != state.maxAverageBrightnessOverride
        || oldState.minBrightnessOverride != state.minBrightnessOverride) {
        Q_EMIT brightnessMetadataChanged();
    }
    if (oldState.sdrGamutWideness != state.sdrGamutWideness) {
        Q_EMIT sdrGamutWidenessChanged();
    }
    if (oldState.vrrPolicy != state.vrrPolicy) {
        Q_EMIT vrrPolicyChanged();
    }
    if (oldState.colorDescription != state.colorDescription) {
        Q_EMIT colorDescriptionChanged();
    }
    if (oldState.colorProfileSource != state.colorProfileSource) {
        Q_EMIT colorProfileSourceChanged();
    }
    if (oldState.brightnessSetting != state.brightnessSetting) {
        Q_EMIT brightnessChanged();
    }
    if (oldState.colorPowerTradeoff != state.colorPowerTradeoff) {
        Q_EMIT colorPowerTradeoffChanged();
    }
    if (oldState.dimming != state.dimming) {
        Q_EMIT dimmingChanged();
    }
    if (oldState.uuid != state.uuid) {
        Q_EMIT uuidChanged();
    }
    if (oldState.replicationSource != state.replicationSource) {
        Q_EMIT replicationSourceChanged();
    }
    // detectedDdcCi is ignored here, it should result in capabilitiesChanged() instead
    if (oldState.allowDdcCi != state.allowDdcCi) {
        Q_EMIT allowDdcCiChanged();
    }
    if (oldState.maxBitsPerColor != state.maxBitsPerColor
        || oldState.automaticMaxBitsPerColorLimit != state.automaticMaxBitsPerColorLimit) {
        Q_EMIT maxBitsPerColorChanged();
    }
    if (oldState.edrPolicy != state.edrPolicy) {
        Q_EMIT edrPolicyChanged();
    }
    if (oldState.blendingColor != state.blendingColor) {
        Q_EMIT blendingColorChanged();
    }
    if (oldState.sharpnessSetting != state.sharpnessSetting) {
        Q_EMIT sharpnessChanged();
    }
    if (oldState.priority != state.priority) {
        Q_EMIT priorityChanged();
    }
    if (oldState.deviceOffset != state.deviceOffset) {
        Q_EMIT deviceOffsetChanged();
    }
    if (oldState.automaticBrightness != state.automaticBrightness) {
        Q_EMIT automaticBrightnessChanged();
    }
    if (oldState.hdrIccProfilePath != state.hdrIccProfilePath) {
        Q_EMIT hdrIccProfilePathChanged();
    }
    if (oldState.hdrColorProfileSource != state.hdrColorProfileSource) {
        Q_EMIT hdrColorProfileSourceChanged();
    }
    if (oldState.abmLevel != state.abmLevel) {
        Q_EMIT abmLevelChanged();
    }
    if (oldState.enabled != state.enabled) {
        Q_EMIT enabledChanged();
    }
}

QSize BackendOutput::orientateSize(const QSize &size) const
{
    switch (m_state.transform.kind()) {
    case OutputTransform::Rotate90:
    case OutputTransform::Rotate270:
    case OutputTransform::FlipX90:
    case OutputTransform::FlipX270:
        return size.transposed();
    default:
        return size;
    }
}

BackendOutput::DpmsMode BackendOutput::dpmsMode() const
{
    return m_state.dpmsMode;
}

uint32_t BackendOutput::overscan() const
{
    return m_state.overscan;
}

VrrPolicy BackendOutput::vrrPolicy() const
{
    return m_state.vrrPolicy;
}

bool BackendOutput::isPlaceholder() const
{
    return m_information.placeholder;
}

bool BackendOutput::isNonDesktop() const
{
    return m_information.nonDesktop;
}

BackendOutput::RgbRange BackendOutput::rgbRange() const
{
    return m_state.rgbRange;
}

void BackendOutput::setChannelFactors(const QVector3D &rgb)
{
}

OutputTransform BackendOutput::panelOrientation() const
{
    return m_information.panelOrientation;
}

bool BackendOutput::wideColorGamut() const
{
    return m_state.wideColorGamut;
}

bool BackendOutput::highDynamicRange() const
{
    return m_state.highDynamicRange;
}

uint32_t BackendOutput::referenceLuminance() const
{
    return m_state.referenceLuminance;
}

BackendOutput::AutoRotationPolicy BackendOutput::autoRotationPolicy() const
{
    return m_state.autoRotatePolicy;
}

std::shared_ptr<IccProfile> BackendOutput::iccProfile() const
{
    return m_state.iccProfile;
}

QString BackendOutput::iccProfilePath() const
{
    return m_state.iccProfilePath;
}

QString BackendOutput::hdrIccProfilePath() const
{
    return m_state.hdrIccProfilePath;
}

QByteArray BackendOutput::mstPath() const
{
    return m_information.mstPath;
}

const std::shared_ptr<ColorDescription> &BackendOutput::colorDescription() const
{
    return m_state.colorDescription;
}

std::optional<double> BackendOutput::advertisedMaxPeakBrightness() const
{
    return m_information.maxPeakBrightness;
}

std::optional<double> BackendOutput::advertisedMaxAverageBrightness() const
{
    return m_information.maxAverageBrightness;
}

double BackendOutput::advertisedMinBrightness() const
{
    return m_information.minBrightness;
}

std::optional<double> BackendOutput::maxPeakBrightnessOverride() const
{
    return m_state.maxPeakBrightnessOverride;
}

std::optional<double> BackendOutput::maxAverageBrightnessOverride() const
{
    return m_state.maxAverageBrightnessOverride;
}

std::optional<double> BackendOutput::minBrightnessOverride() const
{
    return m_state.minBrightnessOverride;
}

double BackendOutput::sdrGamutWideness() const
{
    return m_state.sdrGamutWideness;
}

BackendOutput::ColorProfileSource BackendOutput::colorProfileSource() const
{
    return m_state.colorProfileSource;
}

BackendOutput::ColorProfileSource BackendOutput::hdrColorProfileSource() const
{
    return m_state.hdrColorProfileSource;
}

double BackendOutput::brightnessSetting() const
{
    return m_state.brightnessSetting;
}

double BackendOutput::dimming() const
{
    return m_state.dimming;
}

std::optional<double> BackendOutput::currentBrightness() const
{
    return m_state.currentBrightness;
}

double BackendOutput::currentDimming() const
{
    return m_state.currentDimming;
}

double BackendOutput::maxPossibleArtificialHdrHeadroom() const
{
    return m_state.maxPossibleArtificialHdrHeadroom;
}

double BackendOutput::artificialHdrHeadroom() const
{
    return m_state.artificialHdrHeadroom;
}

BrightnessDevice *BackendOutput::brightnessDevice() const
{
    return m_state.brightnessDevice;
}

void BackendOutput::unsetBrightnessDevice()
{
    State next;
    next.brightnessDevice = nullptr;
    setState(next);
}

bool BackendOutput::allowSdrSoftwareBrightness() const
{
    return m_state.allowSdrSoftwareBrightness;
}

BackendOutput::ColorPowerTradeoff BackendOutput::colorPowerTradeoff() const
{
    return m_state.colorPowerTradeoff;
}

QString BackendOutput::replicationSource() const
{
    return m_state.replicationSource;
}

bool BackendOutput::detectedDdcCi() const
{
    return m_state.detectedDdcCi;
}

bool BackendOutput::allowDdcCi() const
{
    return m_state.allowDdcCi;
}

uint32_t BackendOutput::maxBitsPerColor() const
{
    return m_state.maxBitsPerColor;
}

BackendOutput::BpcRange BackendOutput::bitsPerColorRange() const
{
    return m_information.bitsPerColorRange;
}

std::optional<uint32_t> BackendOutput::automaticMaxBitsPerColorLimit() const
{
    return m_state.automaticMaxBitsPerColorLimit;
}

BackendOutput::EdrPolicy BackendOutput::edrPolicy() const
{
    return m_state.edrPolicy;
}

double BackendOutput::sharpnessSetting() const
{
    return m_state.sharpnessSetting;
}

void BackendOutput::setAutoRotateAvailable(bool isAvailable)
{
}

void BackendOutput::setAutoBrightnessAvailable(bool isAvailable)
{
}

std::optional<uint32_t> BackendOutput::minVrrRefreshRateHz() const
{
    return m_information.minVrrRefreshRateHz;
}

bool BackendOutput::presentAsync(OutputLayer *layer, std::optional<std::chrono::nanoseconds> allowedVrrDelay)
{
    return false;
}

void BackendOutput::repairPresentation()
{
}

const std::shared_ptr<ColorDescription> &BackendOutput::blendingColor() const
{
    return m_state.blendingColor;
}

const std::shared_ptr<ColorDescription> &BackendOutput::layerBlendingColor() const
{
    return m_state.layerBlendingColor;
}

bool BackendOutput::setPostBlendPipeline(const ColorPipeline &, const std::shared_ptr<ColorDescription> &)
{
    return false;
}

void BackendOutput::resetPostBlendPipeline()
{
}

uint32_t BackendOutput::priority() const
{
    return m_state.priority;
}

double BackendOutput::scaleSetting() const
{
    return m_state.scaleSetting;
}

QPoint BackendOutput::deviceOffset() const
{
    return m_state.deviceOffset;
}

// TODO move these quirks to libdisplay-info?
static const std::array s_brokenDdcCi = {
    std::make_pair(QByteArrayLiteral("SAM"), QByteArrayLiteral("Odyssey G5")),
    std::make_pair(QByteArrayLiteral("SAM"), QByteArrayLiteral("LS24D60xU")),
};

bool BackendOutput::isDdcCiKnownBroken() const
{
    return m_information.edid.isValid() && std::ranges::any_of(s_brokenDdcCi, [this](const auto &pair) {
        return m_information.edid.eisaId() == pair.first
            && m_information.edid.monitorName() == pair.second;
    });
}

bool BackendOutput::recommendsOverlayUse() const
{
    return true;
}

const AutoBrightnessCurve &BackendOutput::autoBrightnessCurve() const
{
    return m_state.autoBrightnessCurve;
}

bool BackendOutput::automaticBrightness() const
{
    return m_state.automaticBrightness;
}

BackendOutput::BrightnessReason BackendOutput::lastBrightnessAdjustmentReason() const
{
    return m_state.lastBrightnessAdjustmentReason;
}

QList<OutputModeline> BackendOutput::customModes() const
{
    return m_state.customModes;
}

uint32_t BackendOutput::abmLevel() const
{
    return m_state.abmLevel;
}

RenderLoop *BackendOutput::renderLoop() const
{
    return m_renderLoop.get();
}

DrmDevice *BackendOutput::scanoutDevice() const
{
    return m_scanoutDevice;
}

} // namespace KWin

#include "moc_backendoutput.cpp"
