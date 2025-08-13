/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/backendoutput.h"

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
    std::optional<std::pair<OutputConfiguration, ConfigType>> queryConfig(const QList<BackendOutput *> &outputs, bool isLidClosed, QOrientationReading *orientation, bool isTabletMode);
    void storeConfig(const QList<BackendOutput *> &allOutputs, bool isLidClosed, const OutputConfiguration &config);

    void applyMirroring(OutputConfiguration &config, const QList<BackendOutput *> &outputs);
    bool isAutoRotateActive(const QList<BackendOutput *> &outputs, bool isTabletMode) const;

private:
    OutputConfiguration generateConfig(const QList<BackendOutput *> &outputs, bool isLidClosed);
    void registerOutputs(const QList<BackendOutput *> &outputs);
    void applyOrientationReading(OutputConfiguration &config, const QList<BackendOutput *> &outputs, QOrientationReading *orientation, bool isTabletMode);
    std::optional<OutputConfiguration> generateLidClosedConfig(const QList<BackendOutput *> &outputs);
    std::shared_ptr<OutputMode> chooseMode(BackendOutput *output) const;
    double chooseScale(BackendOutput *output, OutputMode *mode) const;
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
        std::optional<double> scaleSetting;
        std::optional<OutputTransform> transform;
        std::optional<OutputTransform> manualTransform;
        std::optional<uint32_t> overscan;
        std::optional<BackendOutput::RgbRange> rgbRange;
        std::optional<VrrPolicy> vrrPolicy;
        std::optional<bool> highDynamicRange;
        std::optional<uint32_t> referenceLuminance;
        std::optional<bool> wideColorGamut;
        std::optional<BackendOutput::AutoRotationPolicy> autoRotation;
        std::optional<QString> iccProfilePath;
        std::optional<BackendOutput::ColorProfileSource> colorProfileSource;
        std::optional<double> maxPeakBrightnessOverride;
        std::optional<double> maxAverageBrightnessOverride;
        std::optional<double> minBrightnessOverride;
        std::optional<double> sdrGamutWideness;
        std::optional<double> brightness;
        std::optional<bool> allowSdrSoftwareBrightness;
        std::optional<BackendOutput::ColorPowerTradeoff> colorPowerTradeoff;
        std::optional<QString> uuid;
        std::optional<bool> detectedDdcCi;
        std::optional<bool> allowDdcCi;
        std::optional<uint32_t> maxBitsPerColor;
        std::optional<BackendOutput::EdrPolicy> edrPolicy;
        std::optional<double> sharpness;
        std::optional<QList<CustomModeDefinition>> customModes;
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

    OutputConfiguration setupToConfig(Setup *setup, const std::unordered_map<BackendOutput *, size_t> &outputMap) const;
    std::optional<std::pair<Setup *, std::unordered_map<BackendOutput *, size_t>>> findSetup(const QList<BackendOutput *> &outputs, bool lidClosed);
    std::optional<size_t> findOutput(BackendOutput *output, const QList<BackendOutput *> &allOutputs) const;

    QList<OutputState> m_outputs;
    QList<Setup> m_setups;
};
}
