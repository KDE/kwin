/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/output.h"

#include <QPoint>
#include <QSize>
#include <QVector>
#include <memory>
#include <optional>
#include <tuple>
#include <unordered_map>

class QOrientationReading;

namespace KWin
{

class OutputConfiguration;

class OutputConfigurationStore
{
public:
    OutputConfigurationStore();
    ~OutputConfigurationStore();

    enum class ConfigType {
        Preexisting,
        Generated,
    };
    std::optional<std::tuple<OutputConfiguration, QVector<Output *>, ConfigType>> queryConfig(const QVector<Output *> &outputs, bool isLidClosed, QOrientationReading *orientation, bool isTabletMode);

    void storeConfig(const QVector<Output *> &allOutputs, bool isLidClosed, const OutputConfiguration &config, const QVector<Output *> &outputOrder);

    bool isAutoRotateActive(const QVector<Output *> &outputs, bool isTabletMode) const;

private:
    void applyOrientationReading(OutputConfiguration &config, const QVector<Output *> &outputs, QOrientationReading *orientation, bool isTabletMode);
    std::optional<std::pair<OutputConfiguration, QVector<Output *>>> generateLidClosedConfig(const QVector<Output *> &outputs);
    std::pair<OutputConfiguration, QVector<Output *>> generateConfig(const QVector<Output *> &outputs, bool isLidClosed);
    std::shared_ptr<OutputMode> chooseMode(Output *output) const;
    double chooseScale(Output *output, OutputMode *mode) const;
    double targetDpi(Output *output) const;
    void load();
    void save();

    struct ModeData
    {
        QSize size;
        uint32_t refreshRate;
    };
    struct OutputState
    {
        // identification data
        std::optional<QString> edidIdentifier;
        std::optional<QString> connectorName;
        // actual state
        std::optional<ModeData> mode;
        std::optional<double> scale;
        std::optional<OutputTransform> transform;
        std::optional<uint32_t> overscan;
        std::optional<Output::RgbRange> rgbRange;
        std::optional<RenderLoop::VrrPolicy> vrrPolicy;
        std::optional<bool> highDynamicRange;
        std::optional<uint32_t> sdrBrightness;
        std::optional<bool> wideColorGamut;
        std::optional<Output::AutoRotationPolicy> autoRotation;
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
        QVector<SetupState> outputs;
    };

    std::pair<OutputConfiguration, QVector<Output *>> setupToConfig(Setup *setup, const std::unordered_map<Output *, size_t> &outputMap) const;
    std::optional<std::pair<Setup *, std::unordered_map<Output *, size_t>>> findSetup(const QVector<Output *> &outputs, bool lidClosed);
    std::optional<size_t> findOutput(Output *output, const QVector<Output *> &allOutputs) const;

    QVector<OutputState> m_outputs;
    QVector<Setup> m_setups;
};
}
