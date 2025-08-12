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
    std::optional<std::tuple<OutputConfiguration, QList<LogicalOutput *>, ConfigType>> queryConfig(const QList<LogicalOutput *> &outputs, bool isLidClosed, QOrientationReading *orientation, bool isTabletMode);
    void storeConfig(const QList<LogicalOutput *> &allOutputs, bool isLidClosed, const OutputConfiguration &config, const QList<LogicalOutput *> &outputOrder);

    void applyMirroring(OutputConfiguration &config, const QList<LogicalOutput *> &outputs);
    bool isAutoRotateActive(const QList<LogicalOutput *> &outputs, bool isTabletMode) const;

private:
    std::pair<OutputConfiguration, QList<LogicalOutput *>> generateConfig(const QList<LogicalOutput *> &outputs, bool isLidClosed);
    void registerOutputs(const QList<LogicalOutput *> &outputs);
    void applyOrientationReading(OutputConfiguration &config, const QList<LogicalOutput *> &outputs, QOrientationReading *orientation, bool isTabletMode);
    std::optional<std::pair<OutputConfiguration, QList<LogicalOutput *>>> generateLidClosedConfig(const QList<LogicalOutput *> &outputs);
    std::shared_ptr<OutputMode> chooseMode(LogicalOutput *output) const;
    double chooseScale(LogicalOutput *output, OutputMode *mode) const;
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
        std::optional<LogicalOutput::RgbRange> rgbRange;
        std::optional<VrrPolicy> vrrPolicy;
        std::optional<bool> highDynamicRange;
        std::optional<uint32_t> referenceLuminance;
        std::optional<bool> wideColorGamut;
        std::optional<LogicalOutput::AutoRotationPolicy> autoRotation;
        std::optional<QString> iccProfilePath;
        std::optional<LogicalOutput::ColorProfileSource> colorProfileSource;
        std::optional<double> maxPeakBrightnessOverride;
        std::optional<double> maxAverageBrightnessOverride;
        std::optional<double> minBrightnessOverride;
        std::optional<double> sdrGamutWideness;
        std::optional<double> brightness;
        std::optional<bool> allowSdrSoftwareBrightness;
        std::optional<LogicalOutput::ColorPowerTradeoff> colorPowerTradeoff;
        std::optional<QString> uuid;
        std::optional<bool> detectedDdcCi;
        std::optional<bool> allowDdcCi;
        std::optional<uint32_t> maxBitsPerColor;
        std::optional<LogicalOutput::EdrPolicy> edrPolicy;
        std::optional<double> sharpness;
    };
    struct SetupState
    {
        size_t outputIndex;
        QPoint position;
        bool enabled;
        int priority;
        QString replicationSource;
    };
    struct Setup
    {
        bool lidClosed = false;
        QList<SetupState> outputs;
    };

    std::pair<OutputConfiguration, QList<LogicalOutput *>> setupToConfig(Setup *setup, const std::unordered_map<LogicalOutput *, size_t> &outputMap) const;
    std::optional<std::pair<Setup *, std::unordered_map<LogicalOutput *, size_t>>> findSetup(const QList<LogicalOutput *> &outputs, bool lidClosed);
    std::optional<size_t> findOutput(LogicalOutput *output, const QList<LogicalOutput *> &allOutputs) const;

    QList<OutputState> m_outputs;
    QList<Setup> m_setups;
};
}
