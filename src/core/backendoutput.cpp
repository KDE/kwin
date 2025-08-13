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

namespace KWin
{

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

QRect BackendOutput::mapFromGlobal(const QRect &rect) const
{
    return rect.translated(-geometry().topLeft());
}

QRectF BackendOutput::mapFromGlobal(const QRectF &rect) const
{
    return rect.translated(-geometry().topLeft());
}

QRectF BackendOutput::mapToGlobal(const QRectF &rect) const
{
    return rect.translated(geometry().topLeft());
}

QRegion BackendOutput::mapToGlobal(const QRegion &region) const
{
    return region.translated(geometry().topLeft());
}

QPointF BackendOutput::mapToGlobal(const QPointF &pos) const
{
    return pos + geometry().topLeft();
}

QPointF BackendOutput::mapFromGlobal(const QPointF &pos) const
{
    return pos - geometry().topLeft();
}

BackendOutput::Capabilities BackendOutput::capabilities() const
{
    return m_information.capabilities;
}

qreal BackendOutput::scale() const
{
    return m_state.scale;
}

QRect BackendOutput::geometry() const
{
    return QRect(m_state.position, pixelSize() / scale());
}

QRectF BackendOutput::geometryF() const
{
    return QRectF(m_state.position, QSizeF(pixelSize()) / scale());
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
    const QRect oldGeometry = geometry();
    const State oldState = m_state;

    m_state = state;

    if (oldGeometry != geometry()) {
        Q_EMIT geometryChanged();
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

void BackendOutput::setDpmsMode(DpmsMode mode)
{
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

bool BackendOutput::setChannelFactors(const QVector3D &rgb)
{
    return false;
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

std::optional<double> BackendOutput::maxPeakBrightness() const
{
    return m_state.maxPeakBrightnessOverride ? m_state.maxPeakBrightnessOverride : m_information.maxPeakBrightness;
}

std::optional<double> BackendOutput::maxAverageBrightness() const
{
    return m_state.maxAverageBrightnessOverride ? *m_state.maxAverageBrightnessOverride : m_information.maxAverageBrightness;
}

double BackendOutput::minBrightness() const
{
    return m_state.minBrightnessOverride.value_or(m_information.minBrightness);
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

QRect BackendOutput::rect() const
{
    return QRect(QPoint(0, 0), geometry().size());
}

QRectF BackendOutput::rectF() const
{
    return QRectF(QPointF(0, 0), geometryF().size());
}

std::chrono::milliseconds BackendOutput::dimAnimationTime()
{
    // See kscreen.kcfg
    return std::chrono::milliseconds(KSharedConfig::openConfig()->group(QStringLiteral("Effect-Kscreen")).readEntry("Duration", 250));
}

} // namespace KWin

#include "moc_backendoutput.cpp"
