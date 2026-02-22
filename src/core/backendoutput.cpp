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
    : m_luxAtBrightness({0, 0, 0, 0, 0, 0})
{
}

double AutoBrightnessCurve::sample(double lux) const
{
    if (lux >= m_luxAtBrightness.back()) {
        return 1.0;
    }
    for (int i = m_luxAtBrightness.size() - 1; i > 0; i--) {
        const double low = m_luxAtBrightness[i - 1];
        const double high = m_luxAtBrightness[i];
        if (lux >= low && lux <= high) {
            const double range = high - low;
            const double highPercent = i / double(m_luxAtBrightness.size() - 1);
            if (qFuzzyIsNull(range)) {
                return highPercent;
            }
            const double lowPercent = (i - 1) / double(m_luxAtBrightness.size() - 1);
            const double factor = (lux - low) / range;
            return std::clamp(std::lerp(lowPercent, highPercent, factor), 0.0, 1.0);
        }
    }
    // lux is <= m_luxAtBrightness[0] -> always 0% brightness
    return 0;
}

void AutoBrightnessCurve::adjust(double brightness, double lux)
{
    // This is the really difficult part about automatic brightness:
    // Adjust the brightness curve in a way that's intuitive for the user,
    // without the user having to care about how it works

    // in lux
    constexpr double minAbsDifference = 1;
    // unitless factor
    constexpr double minRelativeDifference = 0.1;

    // the constraints on the curve are:
    // - it must be strictly monotonic, so
    //      - m_luxAtBrightness[i] > m_luxAtBrightness[i - 1]
    // - the difference between two control points has to be large enough to avoid small
    //   luminance fluctuations from triggering large brightness jumps. More specifically,
    //      - m_luxAtBrightness[i] >= m_luxAtBrightness[i - 1] * (1 + minRelativeDifference)
    //          - aka m_luxAtBrightness[i] / (1 + minRelativeDifference) >= m_luxAtBrightness[i - 1]
    //      - m_luxAtBrightness[i] >= m_luxAtBrightness[i - 1] + minAbsDifference
    // - after this method ran, adjust(lux) = brightness

    const size_t lowMatch = std::floor(brightness * (m_luxAtBrightness.size() - 1));
    if (lowMatch == m_luxAtBrightness.size() - 1) {
        // == 100% brightness
        m_luxAtBrightness.back() = lux;
        double last = lux;
        for (int i = m_luxAtBrightness.size() - 2; i >= 0; i--) {
            m_luxAtBrightness[i] = std::min({m_luxAtBrightness[i], last / (1 + minRelativeDifference), last - minAbsDifference});
            last = m_luxAtBrightness[i];
        }
        return;
    }

    // Intermediate values between control points are computed using linear interpolation. We want to
    // end up with such low and high control points that when you interpolate between them, you land
    // at the given lux level for the given screen brightness.
    //
    //               (*) high control point
    //               /
    //              /
    //            (*) lux point
    //            /
    //           /
    //         (*) low control point
    //
    // Visually, you can think of the adjustment algorithm as follows: high and low control points
    // are distributed relative to the lux point (i.e. it acts as the pivot point for rotation and scaling)
    // until linear interpolation produces the desired results.
    //
    // Given
    //
    //    lux = low + (high - low) * highFactor
    //
    // The low control point can be found based on the lux and high points as follows:
    //
    //    lux = low + (high - low) * highFactor
    //    -> lux = highFactor * high + (1 - highFactor) * low
    //    -> (1 - highFactor) * low = lux - highFactor * high
    //    -> low = (lux - highFactor * high) / (1 - highFactor)
    //
    // The high control point is chosen using a "sufficiently reasonable heuristic" - the sample point
    // influences the high control point the closer it lies to it.
    //
    // Given that the adjustment algorithm can be viewed as rotation and scaling around a pivot point,
    // in order to satisfy the auto brightness curve constraints, we only need to pick the appropriate
    // coordinates for the high control point.
    //
    // In order to guarantee that `m_luxAtBrightness[i] >= m_luxAtBrightness[i - 1] + minAbsDifference`
    // is satisfied, the high control point should be at least `minAbsDifference * (1 - highFactor)` away
    // from the lux point. In that case, the low control point and the lux point will be
    // `minAbsDifference * highFactor` far apart from each other:
    //
    //     low = (lux - highFactor * high) / (1 - highFactor)
    //         = (lux - highFactor * (lux + minAbsDifference * (1 - highFactor))) / (1 - highFactor)
    //         ...
    //         = lux - minAbsDifference * highFactor
    //
    //     distance(high, low) = high - low
    //                         = (lux + minAbsDifference * (1 - highFactor)) - (lux - minAbsDifference * highFactor)
    //                         = minAbsDifference
    //
    // Similarly, in order to guarantee the `m_luxAtBrightness[i] >= m_luxAtBrightness[i - 1] * (1 + minRelativeDifference)`
    // constraint, the high control point should be at least `minRelativeDifference * (1 - highFactor) * lux`
    // away from the lux point. Then, the low control point and the lux point will be
    // `minRelativeDifference * highFactor * lux` far apart from each other (it can be confirmed by
    // plugging in the corresponding high control point coordinate to the low control point equation).

    double &low = m_luxAtBrightness[lowMatch];
    double &high = m_luxAtBrightness[lowMatch + 1];

    const double highFactor = brightness * (m_luxAtBrightness.size() - 1) - lowMatch;
    high = std::lerp(high, lux, highFactor);

    const double minHighValueRelative = lux + minRelativeDifference * (1 - highFactor) * lux;
    const double minHighValueAbsolute = lux + minAbsDifference * (1 - highFactor);
    high = std::max({minHighValueAbsolute, minHighValueRelative, high});
    low = (lux - highFactor * high) / (1 - highFactor);

    // ensure the curve stays strictly monotonic below the low control point (lowMatch)
    double last = low;
    for (int i = lowMatch - 1; i >= 0; i--) {
        m_luxAtBrightness[i] = std::min({m_luxAtBrightness[i], last / (1 + minRelativeDifference), last - minAbsDifference});
        last = m_luxAtBrightness[i];
    }
    // and do the same above the high control point (lowMatch + 1)
    last = high;
    for (size_t i = lowMatch + 2; i < m_luxAtBrightness.size(); i++) {
        m_luxAtBrightness[i] = std::max({m_luxAtBrightness[i], last * (1 + minRelativeDifference), last + minAbsDifference});
        last = m_luxAtBrightness[i];
    }
}

QJsonArray AutoBrightnessCurve::toArray() const
{
    QJsonArray ret;
    for (const double lux : m_luxAtBrightness) {
        ret.push_back(lux);
    }
    return ret;
}

std::optional<AutoBrightnessCurve> AutoBrightnessCurve::fromArray(const QJsonArray &array)
{
    if (array.size() != s_controlPointCount) {
        return std::nullopt;
    }
    size_t index = 0;
    AutoBrightnessCurve ret;
    for (const auto &value : array) {
        ret.m_luxAtBrightness[index] = value.toDouble(0.0);
        index++;
    }
    return ret;
}

BackendOutput::BackendOutput()
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

QSize BackendOutput::desiredModeSize() const
{
    return m_state.desiredModeSize;
}

uint32_t BackendOutput::desiredModeRefreshRate() const
{
    return m_state.desiredModeRefreshRate;
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
    next.desiredModeSize = props->desiredModeSize.value_or(m_state.desiredModeSize);
    next.desiredModeRefreshRate = props->desiredModeRefreshRate.value_or(m_state.desiredModeRefreshRate);
    next.uuid = props->uuid.value_or(m_state.uuid);
    next.replicationSource = props->replicationSource.value_or(m_state.replicationSource);
    next.priority = props->priority.value_or(m_state.priority);
    next.deviceOffset = props->deviceOffset.value_or(m_state.deviceOffset);

    setState(next);

    Q_EMIT changed();
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
    if (oldState.scaleSetting != state.scaleSetting) {
        Q_EMIT scaleSettingChanged();
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
};

bool BackendOutput::isDdcCiKnownBroken() const
{
    return m_information.edid.isValid() && std::ranges::any_of(s_brokenDdcCi, [this](const auto &pair) {
        return m_information.edid.eisaId() == pair.first
            && m_information.edid.monitorName() == pair.second;
    });
}

bool BackendOutput::overlayLayersLikelyBroken() const
{
    return false;
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

QList<CustomModeDefinition> BackendOutput::customModes() const
{
    return m_state.customModes;
}

} // namespace KWin

#include "moc_backendoutput.cpp"
