/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "outputconfigurationstore.h"
#include "core/inputdevice.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "core/outputconfiguration.h"
#include "input.h"
#include "input_event.h"
#include "kscreenintegration.h"
#include "workspace.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOrientationReading>

namespace KWin
{

OutputConfigurationStore::OutputConfigurationStore()
{
    load();
}

OutputConfigurationStore::~OutputConfigurationStore()
{
    save();
}

std::optional<std::tuple<OutputConfiguration, QList<Output *>, OutputConfigurationStore::ConfigType>> OutputConfigurationStore::queryConfig(const QList<Output *> &outputs, bool isLidClosed, QOrientationReading *orientation, bool isTabletMode)
{
    QList<Output *> relevantOutputs;
    std::copy_if(outputs.begin(), outputs.end(), std::back_inserter(relevantOutputs), [](Output *output) {
        return !output->isNonDesktop() && !output->isPlaceholder();
    });
    if (relevantOutputs.isEmpty()) {
        return std::nullopt;
    }
    if (const auto opt = findSetup(relevantOutputs, isLidClosed)) {
        const auto &[setup, outputStates] = *opt;
        auto [config, order] = setupToConfig(setup, outputStates);
        applyOrientationReading(config, relevantOutputs, orientation, isTabletMode);
        storeConfig(relevantOutputs, isLidClosed, config, order);
        return std::make_tuple(config, order, ConfigType::Preexisting);
    }
    if (auto kscreenConfig = KScreenIntegration::readOutputConfig(relevantOutputs, KScreenIntegration::connectedOutputsHash(relevantOutputs, isLidClosed))) {
        auto &[config, order] = *kscreenConfig;
        applyOrientationReading(config, relevantOutputs, orientation, isTabletMode);
        storeConfig(relevantOutputs, isLidClosed, config, order);
        return std::make_tuple(config, order, ConfigType::Preexisting);
    }
    auto [config, order] = generateConfig(relevantOutputs, isLidClosed);
    applyOrientationReading(config, relevantOutputs, orientation, isTabletMode);
    storeConfig(relevantOutputs, isLidClosed, config, order);
    return std::make_tuple(config, order, ConfigType::Generated);
}

void OutputConfigurationStore::applyOrientationReading(OutputConfiguration &config, const QList<Output *> &outputs, QOrientationReading *orientation, bool isTabletMode)
{
    const auto output = std::find_if(outputs.begin(), outputs.end(), [&config](Output *output) {
        return output->isInternal() && config.changeSet(output)->enabled.value_or(output->isEnabled());
    });
    if (output == outputs.end()) {
        return;
    }
    // TODO move other outputs to matching positions
    const auto changeset = config.changeSet(*output);
    if (!isAutoRotateActive(outputs, isTabletMode)) {
        changeset->transform = changeset->manualTransform;
        return;
    }
    switch (orientation->orientation()) {
    case QOrientationReading::Orientation::TopUp:
        changeset->transform = OutputTransform::Kind::Normal;
        return;
    case QOrientationReading::Orientation::TopDown:
        changeset->transform = OutputTransform::Kind::Rotated180;
        return;
    case QOrientationReading::Orientation::LeftUp:
        changeset->transform = OutputTransform::Kind::Rotated90;
        return;
    case QOrientationReading::Orientation::RightUp:
        changeset->transform = OutputTransform::Kind::Rotated270;
        return;
    case QOrientationReading::Orientation::FaceUp:
    case QOrientationReading::Orientation::FaceDown:
        return;
    case QOrientationReading::Orientation::Undefined:
        changeset->transform = changeset->manualTransform;
        return;
    }
}

std::optional<std::pair<OutputConfigurationStore::Setup *, std::unordered_map<Output *, size_t>>> OutputConfigurationStore::findSetup(const QList<Output *> &outputs, bool lidClosed)
{
    std::unordered_map<Output *, size_t> outputStates;
    for (Output *output : outputs) {
        if (auto opt = findOutput(output, outputs)) {
            outputStates[output] = *opt;
        } else {
            return std::nullopt;
        }
    }
    const auto setup = std::find_if(m_setups.begin(), m_setups.end(), [lidClosed, &outputStates](const auto &setup) {
        if (setup.lidClosed != lidClosed || size_t(setup.outputs.size()) != outputStates.size()) {
            return false;
        }
        return std::all_of(outputStates.begin(), outputStates.end(), [&setup](const auto &outputIt) {
            return std::any_of(setup.outputs.begin(), setup.outputs.end(), [&outputIt](const auto &outputInfo) {
                return outputInfo.outputIndex == outputIt.second;
            });
        });
    });
    if (setup == m_setups.end()) {
        return std::nullopt;
    } else {
        return std::make_pair(&(*setup), outputStates);
    }
}

std::optional<size_t> OutputConfigurationStore::findOutput(Output *output, const QList<Output *> &allOutputs) const
{
    const bool uniqueEdid = !output->edid().identifier().isEmpty() && std::none_of(allOutputs.begin(), allOutputs.end(), [output](Output *otherOutput) {
        return otherOutput != output && otherOutput->edid().identifier() == output->edid().identifier();
    });
    const bool uniqueMst = !output->mstPath().isEmpty() && std::none_of(allOutputs.begin(), allOutputs.end(), [output](Output *otherOutput) {
        return otherOutput != output && otherOutput->edid().identifier() == output->edid().identifier() && otherOutput->mstPath() == output->mstPath();
    });
    const auto it = std::find_if(m_outputs.begin(), m_outputs.end(), [uniqueEdid, uniqueMst, output](const auto &outputState) {
        if (outputState.edidIdentifier != output->edid().identifier()) {
            return false;
        } else if (uniqueEdid) {
            return true;
        }
        if (outputState.mstPath != output->mstPath()) {
            return false;
        } else if (uniqueMst) {
            return true;
        }
        return outputState.connectorName == output->name();
    });
    if (it != m_outputs.end()) {
        return std::distance(m_outputs.begin(), it);
    } else {
        return std::nullopt;
    }
}

void OutputConfigurationStore::storeConfig(const QList<Output *> &allOutputs, bool isLidClosed, const OutputConfiguration &config, const QList<Output *> &outputOrder)
{
    QList<Output *> relevantOutputs;
    std::copy_if(allOutputs.begin(), allOutputs.end(), std::back_inserter(relevantOutputs), [](Output *output) {
        return !output->isNonDesktop() && !output->isPlaceholder();
    });
    if (relevantOutputs.isEmpty()) {
        return;
    }
    const auto opt = findSetup(relevantOutputs, isLidClosed);
    Setup *setup = nullptr;
    if (opt) {
        setup = opt->first;
    } else {
        m_setups.push_back(Setup{});
        setup = &m_setups.back();
        setup->lidClosed = isLidClosed;
    }
    for (Output *output : relevantOutputs) {
        auto outputIndex = findOutput(output, outputOrder);
        if (!outputIndex) {
            m_outputs.push_back(OutputState{});
            outputIndex = m_outputs.size() - 1;
        }
        auto outputIt = std::find_if(setup->outputs.begin(), setup->outputs.end(), [outputIndex](const auto &output) {
            return output.outputIndex == outputIndex;
        });
        if (outputIt == setup->outputs.end()) {
            setup->outputs.push_back(SetupState{});
            outputIt = setup->outputs.end() - 1;
        }
        if (const auto changeSet = config.constChangeSet(output)) {
            std::shared_ptr<OutputMode> mode = changeSet->mode.value_or(output->currentMode()).lock();
            if (!mode) {
                mode = output->currentMode();
            }
            m_outputs[*outputIndex] = OutputState{
                .edidIdentifier = output->edid().identifier(),
                .connectorName = output->name(),
                .mstPath = output->mstPath(),
                .mode = ModeData{
                    .size = mode->size(),
                    .refreshRate = mode->refreshRate(),
                },
                .scale = changeSet->scale.value_or(output->scale()),
                .transform = changeSet->transform.value_or(output->transform()),
                .manualTransform = changeSet->manualTransform.value_or(output->manualTransform()),
                .overscan = changeSet->overscan.value_or(output->overscan()),
                .rgbRange = changeSet->rgbRange.value_or(output->rgbRange()),
                .vrrPolicy = changeSet->vrrPolicy.value_or(output->vrrPolicy()),
                .highDynamicRange = changeSet->highDynamicRange.value_or(output->highDynamicRange()),
                .sdrBrightness = changeSet->sdrBrightness.value_or(output->sdrBrightness()),
                .wideColorGamut = changeSet->wideColorGamut.value_or(output->wideColorGamut()),
                .autoRotation = changeSet->autoRotationPolicy.value_or(output->autoRotationPolicy()),
                .iccProfilePath = changeSet->iccProfilePath.value_or(output->iccProfilePath()),
                .maxPeakBrightnessOverride = changeSet->maxPeakBrightnessOverride.value_or(output->maxPeakBrightnessOverride()),
                .maxAverageBrightnessOverride = changeSet->maxAverageBrightnessOverride.value_or(output->maxAverageBrightnessOverride()),
                .minBrightnessOverride = changeSet->minBrightnessOverride.value_or(output->minBrightnessOverride()),
                .sdrGamutWideness = changeSet->sdrGamutWideness.value_or(output->sdrGamutWideness()),
            };
            *outputIt = SetupState{
                .outputIndex = *outputIndex,
                .position = changeSet->pos.value_or(output->geometry().topLeft()),
                .enabled = changeSet->enabled.value_or(output->isEnabled()),
                .priority = int(outputOrder.indexOf(output)),
            };
        } else {
            const auto mode = output->currentMode();
            m_outputs[*outputIndex] = OutputState{
                .edidIdentifier = output->edid().identifier(),
                .connectorName = output->name(),
                .mstPath = output->mstPath(),
                .mode = ModeData{
                    .size = mode->size(),
                    .refreshRate = mode->refreshRate(),
                },
                .scale = output->scale(),
                .transform = output->transform(),
                .manualTransform = output->manualTransform(),
                .overscan = output->overscan(),
                .rgbRange = output->rgbRange(),
                .vrrPolicy = output->vrrPolicy(),
                .highDynamicRange = output->highDynamicRange(),
                .sdrBrightness = output->sdrBrightness(),
                .wideColorGamut = output->wideColorGamut(),
                .autoRotation = output->autoRotationPolicy(),
                .iccProfilePath = output->iccProfilePath(),
                .maxPeakBrightnessOverride = output->maxPeakBrightnessOverride(),
                .maxAverageBrightnessOverride = output->maxAverageBrightnessOverride(),
                .minBrightnessOverride = output->minBrightnessOverride(),
                .sdrGamutWideness = output->sdrGamutWideness(),
            };
            *outputIt = SetupState{
                .outputIndex = *outputIndex,
                .position = output->geometry().topLeft(),
                .enabled = output->isEnabled(),
                .priority = int(outputOrder.indexOf(output)),
            };
        }
    }
    save();
}

std::pair<OutputConfiguration, QList<Output *>> OutputConfigurationStore::setupToConfig(Setup *setup, const std::unordered_map<Output *, size_t> &outputMap) const
{
    OutputConfiguration ret;
    QList<std::pair<Output *, size_t>> priorities;
    for (const auto &[output, outputIndex] : outputMap) {
        const OutputState &state = m_outputs[outputIndex];
        const auto &setupState = *std::find_if(setup->outputs.begin(), setup->outputs.end(), [outputIndex = outputIndex](const auto &state) {
            return state.outputIndex == outputIndex;
        });
        const auto modes = output->modes();
        const auto mode = std::find_if(modes.begin(), modes.end(), [&state](const auto &mode) {
            return state.mode
                && mode->size() == state.mode->size
                && mode->refreshRate() == state.mode->refreshRate;
        });
        *ret.changeSet(output) = OutputChangeSet{
            .mode = mode == modes.end() ? std::nullopt : std::optional(*mode),
            .enabled = setupState.enabled,
            .pos = setupState.position,
            .scale = state.scale,
            .transform = state.transform,
            .manualTransform = state.manualTransform,
            .overscan = state.overscan,
            .rgbRange = state.rgbRange,
            .vrrPolicy = state.vrrPolicy,
            .highDynamicRange = state.highDynamicRange,
            .sdrBrightness = state.sdrBrightness,
            .wideColorGamut = state.wideColorGamut,
            .autoRotationPolicy = state.autoRotation,
            .iccProfilePath = state.iccProfilePath,
            .maxPeakBrightnessOverride = state.maxPeakBrightnessOverride,
            .maxAverageBrightnessOverride = state.maxAverageBrightnessOverride,
            .minBrightnessOverride = state.minBrightnessOverride,
            .sdrGamutWideness = state.sdrGamutWideness,
        };
        if (setupState.enabled) {
            priorities.push_back(std::make_pair(output, setupState.priority));
        }
    }
    std::sort(priorities.begin(), priorities.end(), [](const auto &left, const auto &right) {
        return left.second < right.second;
    });
    QList<Output *> order;
    std::transform(priorities.begin(), priorities.end(), std::back_inserter(order), [](const auto &pair) {
        return pair.first;
    });
    return std::make_pair(ret, order);
}

std::optional<std::pair<OutputConfiguration, QList<Output *>>> OutputConfigurationStore::generateLidClosedConfig(const QList<Output *> &outputs)
{
    const auto internalIt = std::find_if(outputs.begin(), outputs.end(), [](Output *output) {
        return output->isInternal();
    });
    if (internalIt == outputs.end()) {
        return std::nullopt;
    }
    const auto setup = findSetup(outputs, false);
    if (!setup) {
        return std::nullopt;
    }
    Output *const internalOutput = *internalIt;
    auto [config, order] = setupToConfig(setup->first, setup->second);
    auto internalChangeset = config.changeSet(internalOutput);
    internalChangeset->enabled = false;
    order.removeOne(internalOutput);

    const bool anyEnabled = std::any_of(outputs.begin(), outputs.end(), [&config = config](Output *output) {
        return config.changeSet(output)->enabled.value_or(output->isEnabled());
    });
    if (!anyEnabled) {
        return std::nullopt;
    }

    const auto getSize = [](OutputChangeSet *changeset, Output *output) {
        auto mode = changeset->mode ? changeset->mode->lock() : nullptr;
        if (!mode) {
            mode = output->currentMode();
        }
        const auto scale = changeset->scale.value_or(output->scale());
        return QSize(std::ceil(mode->size().width() / scale), std::ceil(mode->size().height() / scale));
    };
    const QPoint internalPos = internalChangeset->pos.value_or(internalOutput->geometry().topLeft());
    const QSize internalSize = getSize(internalChangeset.get(), internalOutput);
    for (Output *otherOutput : outputs) {
        auto changeset = config.changeSet(otherOutput);
        QPoint otherPos = changeset->pos.value_or(otherOutput->geometry().topLeft());
        if (otherPos.x() >= internalPos.x() + internalSize.width()) {
            otherPos.rx() -= std::floor(internalSize.width());
        }
        if (otherPos.y() >= internalPos.y() + internalSize.height()) {
            otherPos.ry() -= std::floor(internalSize.height());
        }
        // make sure this doesn't make outputs overlap, which is neither supported nor expected by users
        const QSize otherSize = getSize(changeset.get(), otherOutput);
        const bool overlap = std::any_of(outputs.begin(), outputs.end(), [&, &config = config](Output *output) {
            if (otherOutput == output) {
                return false;
            }
            const auto changeset = config.changeSet(output);
            const QPoint pos = changeset->pos.value_or(output->geometry().topLeft());
            return QRect(pos, otherSize).intersects(QRect(otherPos, getSize(changeset.get(), output)));
        });
        if (!overlap) {
            changeset->pos = otherPos;
        }
    }
    return std::make_pair(config, order);
}

std::pair<OutputConfiguration, QList<Output *>> OutputConfigurationStore::generateConfig(const QList<Output *> &outputs, bool isLidClosed)
{
    if (isLidClosed) {
        if (const auto closedConfig = generateLidClosedConfig(outputs)) {
            return *closedConfig;
        }
    }
    OutputConfiguration ret;
    QList<Output *> outputOrder;
    QPoint pos(0, 0);
    for (const auto output : outputs) {
        const auto outputIndex = findOutput(output, outputs);
        const bool enable = !isLidClosed || !output->isInternal() || outputs.size() == 1;
        const OutputState existingData = outputIndex ? m_outputs[*outputIndex] : OutputState{};

        const auto modes = output->modes();
        const auto modeIt = std::find_if(modes.begin(), modes.end(), [&existingData](const auto &mode) {
            return existingData.mode
                && mode->size() == existingData.mode->size
                && mode->refreshRate() == existingData.mode->refreshRate;
        });
        const auto mode = modeIt == modes.end() ? output->currentMode() : *modeIt;

        const auto changeset = ret.changeSet(output);
        *changeset = {
            .mode = mode,
            .enabled = enable,
            .pos = pos,
            .scale = existingData.scale.value_or(chooseScale(output, mode.get())),
            .transform = existingData.transform.value_or(output->panelOrientation()),
            .manualTransform = existingData.manualTransform.value_or(output->panelOrientation()),
            .overscan = existingData.overscan.value_or(0),
            .rgbRange = existingData.rgbRange.value_or(Output::RgbRange::Automatic),
            .vrrPolicy = existingData.vrrPolicy.value_or(RenderLoop::VrrPolicy::Automatic),
            .highDynamicRange = existingData.highDynamicRange.value_or(false),
            .sdrBrightness = existingData.sdrBrightness.value_or(200),
            .wideColorGamut = existingData.wideColorGamut.value_or(false),
            .autoRotationPolicy = existingData.autoRotation.value_or(Output::AutoRotationPolicy::InTabletMode),
        };
        if (enable) {
            pos.setX(std::ceil(pos.x() + changeset->mode.value_or(output->currentMode()).lock()->size().width() / changeset->scale.value_or(output->scale())));
            outputOrder.push_back(output);
        }
    }
    return std::make_pair(ret, outputs);
}

std::shared_ptr<OutputMode> OutputConfigurationStore::chooseMode(Output *output) const
{
    const auto modes = output->modes();

    // some displays advertise bigger modes than their native resolution
    // to avoid that, take the preferred mode into account, which is usually the native one
    const auto preferred = std::find_if(modes.begin(), modes.end(), [](const auto &mode) {
        return mode->flags() & OutputMode::Flag::Preferred;
    });
    if (preferred != modes.end()) {
        // some high refresh rate displays advertise a 60Hz mode as preferred for compatibility reasons
        // ignore that and choose the highest possible refresh rate by default instead
        std::shared_ptr<OutputMode> highestRefresh = *preferred;
        for (const auto &mode : modes) {
            if (mode->size() == highestRefresh->size() && mode->refreshRate() > highestRefresh->refreshRate()) {
                highestRefresh = mode;
            }
        }
        // if the preferred mode size has a refresh rate that's too low for PCs,
        // allow falling back to a mode with lower resolution and a more usable refresh rate
        if (highestRefresh->refreshRate() >= 50000) {
            return highestRefresh;
        }
    }

    std::shared_ptr<OutputMode> ret;
    for (auto mode : modes) {
        if (mode->flags() & OutputMode::Flag::Generated) {
            // generated modes aren't guaranteed to work, so don't choose one as the default
            continue;
        }
        if (!ret) {
            ret = mode;
            continue;
        }
        const bool retUsableRefreshRate = ret->refreshRate() >= 50000;
        const bool usableRefreshRate = mode->refreshRate() >= 50000;
        if (retUsableRefreshRate && !usableRefreshRate) {
            ret = mode;
            continue;
        }
        if ((usableRefreshRate && !retUsableRefreshRate)
            || mode->size().width() > ret->size().width()
            || mode->size().height() > ret->size().height()
            || (mode->size() == ret->size() && mode->refreshRate() > ret->refreshRate())) {
            ret = mode;
        }
    }
    return ret;
}

double OutputConfigurationStore::chooseScale(Output *output, OutputMode *mode) const
{
    if (output->physicalSize().height() <= 0) {
        // invalid size, can't do anything with this
        return 1.0;
    }
    const double outputDpi = mode->size().height() / (output->physicalSize().height() / 25.4);
    const double desiredScale = outputDpi / targetDpi(output);
    // round to 25% steps
    return std::clamp(std::round(100.0 * desiredScale / 25.0) * 25.0 / 100.0, 1.0, 3.0);
}

double OutputConfigurationStore::targetDpi(Output *output) const
{
    // The eye's ability to perceive detail diminishes with distance, so objects
    // that are closer can be smaller and their details remain equally
    // distinguishable. As a result, each device type has its own ideal physical
    // size of items on its screen based on how close the user's eyes are
    // expected to be from it on average, and its target DPI value needs to be
    // changed accordingly.
    const auto devices = input()->devices();
    const bool hasLaptopLid = std::any_of(devices.begin(), devices.end(), [](const auto &device) {
        return device->isLidSwitch();
    });
    if (output->isInternal()) {
        if (hasLaptopLid) {
            // laptop screens: usually closer to the face than desktop monitors
            return 125;
        } else {
            // phone screens: even closer than laptops
            return 136;
        }
    } else {
        // "normal" 1x scale desktop monitor dpi
        return 96;
    }
}

void OutputConfigurationStore::load()
{
    const QString jsonPath = QStandardPaths::locate(QStandardPaths::ConfigLocation, QStringLiteral("kwinoutputconfig.json"));
    if (jsonPath.isEmpty()) {
        return;
    }

    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        qCWarning(KWIN_CORE) << "Could not open file" << jsonPath;
        return;
    }
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qCWarning(KWIN_CORE) << "Failed to parse" << jsonPath << error.errorString();
        return;
    }
    const auto array = doc.array();
    std::vector<QJsonObject> objects;
    std::transform(array.begin(), array.end(), std::back_inserter(objects), [](const auto &json) {
        return json.toObject();
    });
    const auto outputsIt = std::find_if(objects.begin(), objects.end(), [](const auto &obj) {
        return obj["name"].toString() == "outputs" && obj["data"].isArray();
    });
    const auto setupsIt = std::find_if(objects.begin(), objects.end(), [](const auto &obj) {
        return obj["name"].toString() == "setups" && obj["data"].isArray();
    });
    if (outputsIt == objects.end() || setupsIt == objects.end()) {
        return;
    }
    const auto outputs = (*outputsIt)["data"].toArray();

    std::vector<std::optional<OutputState>> outputDatas;
    for (const auto &output : outputs) {
        const auto data = output.toObject();
        OutputState state;
        bool hasIdentifier = false;
        if (const auto it = data.find("edidIdentifier"); it != data.end()) {
            if (const auto str = it->toString(); !str.isEmpty()) {
                state.edidIdentifier = str;
                hasIdentifier = true;
            }
        }
        if (const auto it = data.find("connectorName"); it != data.end()) {
            if (const auto str = it->toString(); !str.isEmpty()) {
                state.connectorName = str;
                hasIdentifier = true;
            }
        }
        if (const auto it = data.find("mstPath"); it != data.end()) {
            if (const auto str = it->toString(); !str.isEmpty()) {
                state.mstPath = str;
                hasIdentifier = true;
            }
        }
        if (!hasIdentifier) {
            // without an identifier the settings are useless
            // we still have to push something into the list so that the indices stay correct
            outputDatas.push_back(std::nullopt);
            qCWarning(KWIN_CORE, "Output in config is missing identifiers");
            continue;
        }
        const bool hasDuplicate = std::any_of(outputDatas.begin(), outputDatas.end(), [&state](const auto &data) {
            return data
                && data->edidIdentifier == state.edidIdentifier
                && data->mstPath == state.mstPath
                && data->connectorName == state.connectorName;
        });
        if (hasDuplicate) {
            qCWarning(KWIN_CORE, "Duplicate output found in config");
            outputDatas.push_back(std::nullopt);
            continue;
        }
        if (const auto it = data.find("mode"); it != data.end()) {
            const auto obj = it->toObject();
            const int width = obj["width"].toInt(0);
            const int height = obj["height"].toInt(0);
            const int refreshRate = obj["refreshRate"].toInt(0);
            if (width > 0 && height > 0 && refreshRate > 0) {
                state.mode = ModeData{
                    .size = QSize(width, height),
                    .refreshRate = uint32_t(refreshRate),
                };
            }
        }
        if (const auto it = data.find("scale"); it != data.end()) {
            const double scale = it->toDouble(0);
            if (scale > 0 && scale <= 3) {
                state.scale = scale;
            }
        }
        if (const auto it = data.find("transform"); it != data.end()) {
            const auto str = it->toString();
            if (str == "Normal") {
                state.transform = state.manualTransform = OutputTransform::Kind::Normal;
            } else if (str == "Rotated90") {
                state.transform = state.manualTransform = OutputTransform::Kind::Rotated90;
            } else if (str == "Rotated180") {
                state.transform = state.manualTransform = OutputTransform::Kind::Rotated180;
            } else if (str == "Rotated270") {
                state.transform = state.manualTransform = OutputTransform::Kind::Rotated270;
            } else if (str == "Flipped") {
                state.transform = state.manualTransform = OutputTransform::Kind::Flipped;
            } else if (str == "Flipped90") {
                state.transform = state.manualTransform = OutputTransform::Kind::Flipped90;
            } else if (str == "Flipped180") {
                state.transform = state.manualTransform = OutputTransform::Kind::Flipped180;
            } else if (str == "Flipped270") {
                state.transform = state.manualTransform = OutputTransform::Kind::Flipped270;
            }
        }
        if (const auto it = data.find("overscan"); it != data.end()) {
            const int overscan = it->toInt(-1);
            if (overscan >= 0 && overscan <= 100) {
                state.overscan = overscan;
            }
        }
        if (const auto it = data.find("rgbRange"); it != data.end()) {
            const auto str = it->toString();
            if (str == "Automatic") {
                state.rgbRange = Output::RgbRange::Automatic;
            } else if (str == "Limited") {
                state.rgbRange = Output::RgbRange::Limited;
            } else if (str == "Full") {
                state.rgbRange = Output::RgbRange::Full;
            }
        }
        if (const auto it = data.find("vrrPolicy"); it != data.end()) {
            const auto str = it->toString();
            if (str == "Never") {
                state.vrrPolicy = RenderLoop::VrrPolicy::Never;
            } else if (str == "Automatic") {
                state.vrrPolicy = RenderLoop::VrrPolicy::Automatic;
            } else if (str == "Always") {
                state.vrrPolicy = RenderLoop::VrrPolicy::Always;
            }
        }
        if (const auto it = data.find("highDynamicRange"); it != data.end() && it->isBool()) {
            state.highDynamicRange = it->toBool();
        }
        if (const auto it = data.find("sdrBrightness"); it != data.end() && it->isDouble()) {
            state.sdrBrightness = it->toInt(200);
        }
        if (const auto it = data.find("wideColorGamut"); it != data.end() && it->isBool()) {
            state.wideColorGamut = it->toBool();
        }
        if (const auto it = data.find("autoRotation"); it != data.end()) {
            const auto str = it->toString();
            if (str == "Never") {
                state.autoRotation = Output::AutoRotationPolicy::Never;
            } else if (str == "InTabletMode") {
                state.autoRotation = Output::AutoRotationPolicy::InTabletMode;
            } else if (str == "Always") {
                state.autoRotation = Output::AutoRotationPolicy::Always;
            }
        }
        if (const auto it = data.find("iccProfilePath"); it != data.end()) {
            state.iccProfilePath = it->toString();
        }
        if (const auto it = data.find("maxPeakBrightnessOverride"); it != data.end() && it->isDouble()) {
            state.maxPeakBrightnessOverride = it->toDouble();
        }
        if (const auto it = data.find("maxAverageBrightnessOverride"); it != data.end() && it->isDouble()) {
            state.maxAverageBrightnessOverride = it->toDouble();
        }
        if (const auto it = data.find("minBrightnessOverride"); it != data.end() && it->isDouble()) {
            state.minBrightnessOverride = it->toDouble();
        }
        if (const auto it = data.find("sdrGamutWideness"); it != data.end() && it->isDouble()) {
            state.sdrGamutWideness = it->toDouble();
        }
        outputDatas.push_back(state);
    }

    const auto setups = (*setupsIt)["data"].toArray();
    for (const auto &s : setups) {
        const auto data = s.toObject();
        const auto outputs = data["outputs"].toArray();
        Setup setup;
        bool fail = false;
        for (const auto &output : outputs) {
            const auto outputData = output.toObject();
            SetupState state;
            if (const auto it = outputData.find("enabled"); it != outputData.end() && it->isBool()) {
                state.enabled = it->toBool();
            } else {
                fail = true;
                break;
            }
            if (const auto it = outputData.find("outputIndex"); it != outputData.end()) {
                const int index = it->toInt(-1);
                if (index <= -1 || size_t(index) >= outputDatas.size()) {
                    fail = true;
                    break;
                }
                // the outputs must be unique
                const bool unique = std::none_of(setup.outputs.begin(), setup.outputs.end(), [&index](const auto &output) {
                    return output.outputIndex == size_t(index);
                });
                if (!unique) {
                    fail = true;
                    break;
                }
                state.outputIndex = index;
            }
            if (const auto it = outputData.find("position"); it != outputData.end()) {
                const auto obj = it->toObject();
                const auto x = obj.find("x");
                const auto y = obj.find("y");
                if (x == obj.end() || !x->isDouble() || y == obj.end() || !y->isDouble()) {
                    fail = true;
                    break;
                }
                state.position = QPoint(x->toInt(0), y->toInt(0));
            } else {
                fail = true;
                break;
            }
            if (const auto it = outputData.find("priority"); it != outputData.end()) {
                state.priority = it->toInt(-1);
                if (state.priority < 0 && state.enabled) {
                    fail = true;
                    break;
                }
            }
            setup.outputs.push_back(state);
        }
        if (fail || setup.outputs.empty()) {
            continue;
        }
        // one of the outputs must be enabled
        const bool noneEnabled = std::none_of(setup.outputs.begin(), setup.outputs.end(), [](const auto &output) {
            return output.enabled;
        });
        if (noneEnabled) {
            continue;
        }
        setup.lidClosed = data["lidClosed"].toBool(false);
        // there must be only one setup that refers to a given set of outputs
        const bool alreadyExists = std::any_of(m_setups.begin(), m_setups.end(), [&setup](const auto &other) {
            if (setup.lidClosed != other.lidClosed || setup.outputs.size() != other.outputs.size()) {
                return false;
            }
            return std::all_of(setup.outputs.begin(), setup.outputs.end(), [&other](const auto &output) {
                return std::any_of(other.outputs.begin(), other.outputs.end(), [&output](const auto &otherOutput) {
                    return output.outputIndex == otherOutput.outputIndex;
                });
            });
        });
        if (alreadyExists) {
            continue;
        }
        m_setups.push_back(setup);
    }

    // repair the outputs list in case it's broken
    for (size_t i = 0; i < outputDatas.size();) {
        if (!outputDatas[i]) {
            outputDatas.erase(outputDatas.begin() + i);
            for (auto setupIt = m_setups.begin(); setupIt != m_setups.end();) {
                const bool broken = std::any_of(setupIt->outputs.begin(), setupIt->outputs.end(), [i](const auto &output) {
                    return output.outputIndex == i;
                });
                if (broken) {
                    setupIt = m_setups.erase(setupIt);
                    continue;
                }
                for (auto &output : setupIt->outputs) {
                    if (output.outputIndex > i) {
                        output.outputIndex--;
                    }
                }
                setupIt++;
            }
        } else {
            i++;
        }
    }

    for (const auto &o : outputDatas) {
        Q_ASSERT(o);
        m_outputs.push_back(*o);
    }
}

void OutputConfigurationStore::save()
{
    QJsonDocument document;
    QJsonArray array;
    QJsonObject outputs;
    outputs["name"] = "outputs";
    QJsonArray outputsData;
    for (const auto &output : m_outputs) {
        QJsonObject o;
        if (output.edidIdentifier) {
            o["edidIdentifier"] = *output.edidIdentifier;
        }
        if (output.connectorName) {
            o["connectorName"] = *output.connectorName;
        }
        if (!output.mstPath.isEmpty()) {
            o["mstPath"] = output.mstPath;
        }
        if (output.mode) {
            QJsonObject mode;
            mode["width"] = output.mode->size.width();
            mode["height"] = output.mode->size.height();
            mode["refreshRate"] = int(output.mode->refreshRate);
            o["mode"] = mode;
        }
        if (output.scale) {
            o["scale"] = *output.scale;
        }
        if (output.manualTransform == OutputTransform::Kind::Normal) {
            o["transform"] = "Normal";
        } else if (output.manualTransform == OutputTransform::Kind::Rotated90) {
            o["transform"] = "Rotated90";
        } else if (output.manualTransform == OutputTransform::Kind::Rotated180) {
            o["transform"] = "Rotated180";
        } else if (output.manualTransform == OutputTransform::Kind::Rotated270) {
            o["transform"] = "Rotated270";
        } else if (output.manualTransform == OutputTransform::Kind::Flipped) {
            o["transform"] = "Flipped";
        } else if (output.manualTransform == OutputTransform::Kind::Flipped90) {
            o["transform"] = "Flipped90";
        } else if (output.manualTransform == OutputTransform::Kind::Flipped180) {
            o["transform"] = "Flipped180";
        } else if (output.manualTransform == OutputTransform::Kind::Flipped270) {
            o["transform"] = "Flipped270";
        }
        if (output.overscan) {
            o["overscan"] = int(*output.overscan);
        }
        if (output.rgbRange == Output::RgbRange::Automatic) {
            o["rgbRange"] = "Automatic";
        } else if (output.rgbRange == Output::RgbRange::Limited) {
            o["rgbRange"] = "Limited";
        } else if (output.rgbRange == Output::RgbRange::Full) {
            o["rgbRange"] = "Full";
        }
        if (output.vrrPolicy == RenderLoop::VrrPolicy::Never) {
            o["vrrPolicy"] = "Never";
        } else if (output.vrrPolicy == RenderLoop::VrrPolicy::Automatic) {
            o["vrrPolicy"] = "Automatic";
        } else if (output.vrrPolicy == RenderLoop::VrrPolicy::Always) {
            o["vrrPolicy"] = "Always";
        }
        if (output.highDynamicRange) {
            o["highDynamicRange"] = *output.highDynamicRange;
        }
        if (output.sdrBrightness) {
            o["sdrBrightness"] = int(*output.sdrBrightness);
        }
        if (output.wideColorGamut) {
            o["wideColorGamut"] = *output.wideColorGamut;
        }
        if (output.autoRotation) {
            switch (*output.autoRotation) {
            case Output::AutoRotationPolicy::Never:
                o["autoRotation"] = "Never";
                break;
            case Output::AutoRotationPolicy::InTabletMode:
                o["autoRotation"] = "InTabletMode";
                break;
            case Output::AutoRotationPolicy::Always:
                o["autoRotation"] = "Always";
                break;
            }
        }
        if (output.iccProfilePath) {
            o["iccProfilePath"] = *output.iccProfilePath;
        }
        if (output.maxPeakBrightnessOverride) {
            o["maxPeakBrightnessOverride"] = *output.maxPeakBrightnessOverride;
        }
        if (output.maxAverageBrightnessOverride) {
            o["maxAverageBrightnessOverride"] = *output.maxAverageBrightnessOverride;
        }
        if (output.minBrightnessOverride) {
            o["minBrightnessOverride"] = *output.minBrightnessOverride;
        }
        if (output.sdrGamutWideness) {
            o["sdrGamutWideness"] = *output.sdrGamutWideness;
        }
        outputsData.append(o);
    }
    outputs["data"] = outputsData;
    array.append(outputs);

    QJsonObject setups;
    setups["name"] = "setups";
    QJsonArray setupData;
    for (const auto &setup : m_setups) {
        QJsonObject o;
        o["lidClosed"] = setup.lidClosed;
        QJsonArray outputs;
        for (ssize_t i = 0; i < setup.outputs.size(); i++) {
            const auto &output = setup.outputs[i];
            QJsonObject o;
            o["enabled"] = output.enabled;
            o["outputIndex"] = int(output.outputIndex);
            o["priority"] = output.priority;
            QJsonObject pos;
            pos["x"] = output.position.x();
            pos["y"] = output.position.y();
            o["position"] = pos;

            outputs.append(o);
        }
        o["outputs"] = outputs;

        setupData.append(o);
    }
    setups["data"] = setupData;
    array.append(setups);

    const QString path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/kwinoutputconfig.json";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qCWarning(KWIN_CORE, "Couldn't open output config file %s", qPrintable(path));
        return;
    }
    document.setArray(array);
    f.write(document.toJson());
    f.flush();
}

bool OutputConfigurationStore::isAutoRotateActive(const QList<Output *> &outputs, bool isTabletMode) const
{
    const auto internalIt = std::find_if(outputs.begin(), outputs.end(), [](Output *output) {
        return output->isInternal() && output->isEnabled();
    });
    if (internalIt == outputs.end()) {
        return false;
    }
    Output *internal = *internalIt;
    switch (internal->autoRotationPolicy()) {
    case Output::AutoRotationPolicy::Never:
        return false;
    case Output::AutoRotationPolicy::InTabletMode:
        return isTabletMode;
    case Output::AutoRotationPolicy::Always:
        return true;
    }
    Q_UNREACHABLE();
}
}
