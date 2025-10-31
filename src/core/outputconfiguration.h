/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"

#include "output.h"

#include <QPoint>
#include <QSize>

namespace KWin
{

class IccProfile;
class BrightnessDevice;

class KWIN_EXPORT OutputChangeSet
{
public:
    std::optional<std::weak_ptr<OutputMode>> mode;
    std::optional<QSize> desiredModeSize;
    std::optional<uint32_t> desiredModeRefreshRate;
    std::optional<bool> enabled;
    std::optional<QPoint> pos;
    std::optional<double> scale;
    std::optional<OutputTransform> transform;
    std::optional<OutputTransform> manualTransform;
    std::optional<uint32_t> overscan;
    std::optional<Output::RgbRange> rgbRange;
    std::optional<VrrPolicy> vrrPolicy;
    std::optional<bool> highDynamicRange;
    std::optional<uint32_t> referenceLuminance;
    std::optional<bool> wideColorGamut;
    std::optional<Output::AutoRotationPolicy> autoRotationPolicy;
    std::optional<QString> iccProfilePath;
    std::optional<std::shared_ptr<IccProfile>> iccProfile;
    std::optional<std::optional<double>> maxPeakBrightnessOverride;
    std::optional<std::optional<double>> maxAverageBrightnessOverride;
    std::optional<std::optional<double>> minBrightnessOverride;
    std::optional<double> sdrGamutWideness;
    std::optional<Output::ColorProfileSource> colorProfileSource;
    std::optional<double> brightness;
    // setting "brightness" may trigger animations;
    // setting the current brightness doesn't
    std::optional<double> currentBrightness;
    std::optional<bool> allowSdrSoftwareBrightness;
    std::optional<Output::ColorPowerTradeoff> colorPowerTradeoff;
    std::optional<double> dimming;
    std::optional<BrightnessDevice *> brightnessDevice;
    std::optional<QString> uuid;
    std::optional<QString> replicationSource;
    std::optional<bool> detectedDdcCi;
    std::optional<bool> allowDdcCi;
    std::optional<uint32_t> maxBitsPerColor;
    std::optional<Output::EdrPolicy> edrPolicy;
    std::optional<double> sharpness;
};

class KWIN_EXPORT OutputConfiguration
{
public:
    std::shared_ptr<OutputChangeSet> changeSet(Output *output);
    std::shared_ptr<OutputChangeSet> constChangeSet(Output *output) const;

private:
    QMap<Output *, std::shared_ptr<OutputChangeSet>> m_properties;
};

}
