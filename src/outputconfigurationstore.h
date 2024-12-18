/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/output.h"

#include <QList>
#include <QPoint>
#include <QSize>
#include <memory>
#include <optional>
#include <tuple>
#include <unordered_map>

class QOrientationReading;

namespace KWin
{

class OutputConfiguration;

class KWIN_EXPORT OutputConfigurationStore
{
public:
    OutputConfigurationStore();
    ~OutputConfigurationStore();

    enum class ConfigType {
        Preexisting,
        Generated,
    };
    std::optional<std::tuple<OutputConfiguration, QList<Output *>, ConfigType>> queryConfig(const QList<Output *> &outputs, bool isLidClosed, QOrientationReading *orientation, bool isTabletMode);
    void storeConfig(const QList<Output *> &allOutputs, bool isLidClosed, const OutputConfiguration &config, const QList<Output *> &outputOrder);
    std::pair<OutputConfiguration, QList<Output *>> generateConfig(const QList<Output *> &outputs, bool isLidClosed);

    bool isAutoRotateActive(const QList<Output *> &outputs, bool isTabletMode) const;

private:
    void applyOrientationReading(OutputConfiguration &config, const QList<Output *> &outputs, QOrientationReading *orientation, bool isTabletMode);
    std::optional<std::pair<OutputConfiguration, QList<Output *>>> generateLidClosedConfig(const QList<Output *> &outputs);
    std::shared_ptr<OutputMode> chooseMode(Output *output) const;
    double chooseScale(Output *output, OutputMode *mode) const;
    void load();
    void save();

    struct ModeData
    {
        QSize size;
        uint32_t refreshRate;
    };
    struct OutputState
    {
        // identification data. Empty if invalid
        QString edidIdentifier;
        QString connectorName;
        QString edidHash;
        QString mstPath;
        // actual state
        std::optional<ModeData> mode;
        std::optional<double> scale;
        std::optional<OutputTransform> transform;
        std::optional<OutputTransform> manualTransform;
        std::optional<uint32_t> overscan;
        std::optional<Output::RgbRange> rgbRange;
        std::optional<VrrPolicy> vrrPolicy;
        std::optional<bool> highDynamicRange;
        std::optional<uint32_t> referenceLuminance;
        std::optional<bool> wideColorGamut;
        std::optional<Output::AutoRotationPolicy> autoRotation;
        std::optional<QString> iccProfilePath;
        std::optional<Output::ColorProfileSource> colorProfileSource;
        std::optional<double> maxPeakBrightnessOverride;
        std::optional<double> maxAverageBrightnessOverride;
        std::optional<double> minBrightnessOverride;
        std::optional<double> sdrGamutWideness;
        std::optional<double> brightness;
        std::optional<bool> allowSdrSoftwareBrightness;
        std::optional<Output::ColorPowerTradeoff> colorPowerTradeoff;
    };
    struct SetupState
    {
        size_t outputIndex;
        QPoint position;
        bool enabled;
        int priority;
    };
    struct Setup
    {
        bool lidClosed = false;
        QList<SetupState> outputs;
    };

    std::pair<OutputConfiguration, QList<Output *>> setupToConfig(Setup *setup, const std::unordered_map<Output *, size_t> &outputMap) const;
    std::optional<std::pair<Setup *, std::unordered_map<Output *, size_t>>> findSetup(const QList<Output *> &outputs, bool lidClosed);
    std::optional<size_t> findOutput(Output *output, const QList<Output *> &allOutputs) const;

    QList<OutputState> m_outputs;
    QList<Setup> m_setups;
};
}
