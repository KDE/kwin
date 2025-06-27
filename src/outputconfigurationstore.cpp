/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "outputconfigurationstore.h"
#include "core/iccprofile.h"
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
#include <ranges>

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
    // assigns uuids, if the outputs don't have one yet
    registerOutputs(outputs);
    if (const auto opt = findSetup(relevantOutputs, isLidClosed)) {
        const auto &[setup, outputStates] = *opt;
        auto [config, order] = setupToConfig(setup, outputStates);
        applyOrientationReading(config, relevantOutputs, orientation, isTabletMode);
        applyMirroring(config, relevantOutputs);
        storeConfig(relevantOutputs, isLidClosed, config, order);
        return std::make_tuple(config, order, ConfigType::Preexisting);
    }
    auto [config, order] = generateConfig(relevantOutputs, isLidClosed);
    applyOrientationReading(config, relevantOutputs, orientation, isTabletMode);
    applyMirroring(config, relevantOutputs);
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
    const auto panelOrientation = (*output)->panelOrientation();
    switch (orientation->orientation()) {
    case QOrientationReading::Orientation::TopUp:
        changeset->transform = panelOrientation;
        return;
    case QOrientationReading::Orientation::TopDown:
        changeset->transform = panelOrientation.combine(OutputTransform::Kind::Rotate180);
        return;
    case QOrientationReading::Orientation::LeftUp:
        changeset->transform = panelOrientation.combine(OutputTransform::Kind::Rotate90);
        return;
    case QOrientationReading::Orientation::RightUp:
        changeset->transform = panelOrientation.combine(OutputTransform::Kind::Rotate270);
        return;
    case QOrientationReading::Orientation::FaceUp:
    case QOrientationReading::Orientation::FaceDown:
        return;
    case QOrientationReading::Orientation::Undefined:
        changeset->transform = changeset->manualTransform;
        return;
    }
}

void OutputConfigurationStore::applyMirroring(OutputConfiguration &config, const QList<Output *> &outputs)
{
    for (const auto output : outputs) {
        const auto changeset = config.changeSet(output);
        const QString mirrorSource = changeset->replicationSource.value_or(output->replicationSource());
        if (mirrorSource.isEmpty()) {
            continue;
        }
        const auto sourceIt = std::ranges::find_if(outputs, [&](Output *o) {
            return config.changeSet(o)->uuid.value_or(o->uuid()) == mirrorSource;
        });
        if (sourceIt == outputs.end()) {
            continue;
        }
        Output *const source = *sourceIt;
        if (source == output) {
            qCWarning(KWIN_CORE, "Output %s is trying to mirror itself, that shouldn't happen!", qPrintable(output->name()));
            continue;
        }
        const auto sourceChange = config.changeSet(source);
        auto sourceMode = sourceChange->mode.value_or(source->currentMode()).lock();
        if (!sourceMode) {
            continue;
        }
        const auto sourcePosition = sourceChange->pos.value_or(source->geometry().topLeft());
        const auto sourceScale = sourceChange->scale.value_or(source->scale());
        const auto sourceTransform = sourceChange->transform.value_or(source->transform());
        const auto sourceModeSize = sourceTransform.map(sourceMode->size());
        const auto logicalSourceSize = sourceModeSize / sourceScale;
        const auto transform = changeset->transform.value_or(output->transform());
        const auto modes = output->modes();
        auto sameAspect = modes | std::views::filter([sourceModeSize, transform](const auto &mode) {
            const double aspect1 = sourceModeSize.width() / double(sourceModeSize.height());
            const auto size = transform.map(mode->size());
            const double aspect2 = size.width() / double(size.height());
            return std::abs(aspect1 - aspect2) < 0.01;
        });
        if (!sameAspect.empty()) {
            changeset->mode = sameAspect.front();
            changeset->scale = std::round(transform.map(sameAspect.front()->size()).width() / double(logicalSourceSize.width()) * 120) / 120.0;
            changeset->pos = sourcePosition;
            continue;
        }
        const auto mode = changeset->mode.value_or(output->currentMode()).lock();
        if (!mode) {
            continue;
        }
        // we can't do anything good here without additional infrastructure to handle mirroring explicitly
        // next-best workaround for now: make the outputs equal height or width through scaling, and then center it on the other axis
        const auto modeSize = transform.map(mode->size());
        if (modeSize.width() > modeSize.height()) {
            changeset->scale = std::round(modeSize.height() / double(logicalSourceSize.height()) * 120) / 120.0;
            const QSize logicalSize = modeSize / *changeset->scale;
            changeset->pos = QPoint(sourcePosition.x() + logicalSourceSize.width() / 2 - logicalSize.width() / 2, sourcePosition.y());
        } else {
            changeset->scale = std::round(modeSize.width() / double(logicalSourceSize.width()) * 120) / 120.0;
            const QSize logicalSize = modeSize / *changeset->scale;
            changeset->pos = QPoint(sourcePosition.x(), sourcePosition.y() + logicalSourceSize.height() / 2 - logicalSize.height() / 2);
        }
    }
    // the fallback logic above may push some output into negative coordinates,
    // which causes issues with Xwayland -> shift all of them to be positive
    for (Output *output : outputs) {
        const auto changeset = config.changeSet(output);
        if (!changeset->pos.has_value()) {
            continue;
        }
        const QPoint pos = *changeset->pos;
        if (pos.x() >= 0 && pos.y() >= 0) {
            continue;
        }
        for (Output *output : outputs) {
            const auto changeset = config.changeSet(output);
            const auto otherPos = changeset->pos.value_or(output->geometry().topLeft());
            changeset->pos = otherPos - QPoint(std::min(pos.x(), 0), std::min(pos.y(), 0));
        }
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
    struct Properties
    {
        std::optional<QString> edidIdentifier;
        std::optional<QString> edidHash;
        std::optional<QString> mstPath;
        std::optional<QString> connectorName;
    };
    const auto filterBy = [this](const Properties &props) {
        std::vector<size_t> ret;
        for (ssize_t i = 0; i < m_outputs.size(); i++) {
            const auto &state = m_outputs[i];
            if (props.edidIdentifier.has_value() && state.edidIdentifier != *props.edidIdentifier) {
                continue;
            }
            if (props.edidHash.has_value() && state.edidHash != *props.edidHash) {
                continue;
            }
            if (props.mstPath.has_value() && state.mstPath != *props.mstPath) {
                continue;
            }
            if (props.connectorName.has_value() && state.connectorName != *props.connectorName) {
                continue;
            }
            ret.push_back(i);
        }
        return ret;
    };

    const bool edidIdUniqueAmongOutputs = !output->edid().identifier().isEmpty() && std::ranges::count_if(allOutputs, [output](Output *otherOutput) {
        return output->edid().identifier() == otherOutput->edid().identifier();
    }) == 1;
    const bool edidHashUniqueAmongOutputs = std::ranges::count_if(allOutputs, [output](Output *otherOutput) {
        return output->edid().hash() == otherOutput->edid().hash();
    }) == 1;
    const bool mstPathUniqueAmongOutputs = !output->mstPath().isEmpty() && std::ranges::count_if(allOutputs, [output](Output *other) {
        return output->edid().hash() == other->edid().hash()
            && output->mstPath() == other->mstPath();
    }) == 1;

    QString edidId = output->edid().identifier();
    auto matches = filterBy(Properties{
        .edidIdentifier = edidId,
        .edidHash = std::nullopt,
        .mstPath = std::nullopt,
        .connectorName = std::nullopt,
    });
    if (matches.size() == 1 && edidIdUniqueAmongOutputs) {
        // if have an EDID ID that's unique in both outputs and config,
        // that's enough information to match the output on its own
        return matches.front();
    } else if (matches.empty()) {
        // special case: if we failed to parse the EDID in the past,
        // we would have no EDID ID but a matching hash
        edidId = QString();
    }
    // either the EDID ID is not unique, or it's missing entirely
    if (edidHashUniqueAmongOutputs) {
        // -> narrow down the search with the EDID ID
        matches = filterBy(Properties{
            .edidIdentifier = edidId,
            .edidHash = output->edid().hash(),
            .mstPath = std::nullopt,
            .connectorName = std::nullopt,
        });
        if (matches.size() == 1) {
            return matches.front();
        } else if (matches.empty()) {
            return std::nullopt;
        }
    }
    if (mstPathUniqueAmongOutputs) {
        // -> narrow down the search with the MST PATH
        matches = filterBy(Properties{
            .edidIdentifier = edidId,
            .edidHash = output->edid().hash(),
            .mstPath = output->mstPath(),
            .connectorName = std::nullopt,
        });
        if (matches.size() == 1) {
            return matches.front();
        }
    }
    // narrow down the search with the connector name as well
    matches = filterBy(Properties{
        .edidIdentifier = edidId,
        .edidHash = output->edid().hash(),
        .mstPath = output->mstPath(),
        .connectorName = output->name(),
    });
    if (!matches.empty()) {
        // there should never multiple entries where all properties are the same
        // so it's safe to just return the first one
        return matches.front();
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
    registerOutputs(allOutputs);
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
        Q_ASSERT(outputIndex.has_value());
        auto outputIt = std::find_if(setup->outputs.begin(), setup->outputs.end(), [outputIndex](const auto &output) {
            return output.outputIndex == outputIndex;
        });
        if (outputIt == setup->outputs.end()) {
            setup->outputs.push_back(SetupState{});
            outputIt = setup->outputs.end() - 1;
        }
        const std::optional<QString> existingUuid = m_outputs[*outputIndex].uuid;
        if (const auto changeSet = config.constChangeSet(output)) {
            QSize modeSize = changeSet->desiredModeSize.value_or(output->desiredModeSize());
            if (modeSize.isEmpty()) {
                modeSize = output->currentMode()->size();
            }
            uint32_t refreshRate = changeSet->desiredModeRefreshRate.value_or(output->desiredModeRefreshRate());
            if (refreshRate == 0) {
                refreshRate = output->currentMode()->refreshRate();
            }
            m_outputs[*outputIndex] = OutputState{
                .edidIdentifier = output->edid().identifier(),
                .connectorName = output->name(),
                .edidHash = output->edid().hash(),
                .mstPath = output->mstPath(),
                .mode = ModeData{
                    .size = modeSize,
                    .refreshRate = refreshRate,
                },
                .scale = changeSet->scale.value_or(output->scale()),
                .transform = changeSet->transform.value_or(output->transform()),
                .manualTransform = changeSet->manualTransform.value_or(output->manualTransform()),
                .overscan = changeSet->overscan.value_or(output->overscan()),
                .rgbRange = changeSet->rgbRange.value_or(output->rgbRange()),
                .vrrPolicy = changeSet->vrrPolicy.value_or(output->vrrPolicy()),
                .highDynamicRange = changeSet->highDynamicRange.value_or(output->highDynamicRange()),
                .referenceLuminance = changeSet->referenceLuminance.value_or(output->referenceLuminance()),
                .wideColorGamut = changeSet->wideColorGamut.value_or(output->wideColorGamut()),
                .autoRotation = changeSet->autoRotationPolicy.value_or(output->autoRotationPolicy()),
                .iccProfilePath = changeSet->iccProfilePath.value_or(output->iccProfilePath()),
                .colorProfileSource = changeSet->colorProfileSource.value_or(output->colorProfileSource()),
                .maxPeakBrightnessOverride = changeSet->maxPeakBrightnessOverride.value_or(output->maxPeakBrightnessOverride()),
                .maxAverageBrightnessOverride = changeSet->maxAverageBrightnessOverride.value_or(output->maxAverageBrightnessOverride()),
                .minBrightnessOverride = changeSet->minBrightnessOverride.value_or(output->minBrightnessOverride()),
                .sdrGamutWideness = changeSet->sdrGamutWideness.value_or(output->sdrGamutWideness()),
                .brightness = changeSet->brightness.value_or(output->brightnessSetting()),
                .allowSdrSoftwareBrightness = changeSet->allowSdrSoftwareBrightness.value_or(output->allowSdrSoftwareBrightness()),
                .colorPowerTradeoff = changeSet->colorPowerTradeoff.value_or(output->colorPowerTradeoff()),
                .uuid = existingUuid,
                .detectedDdcCi = changeSet->detectedDdcCi.value_or(output->detectedDdcCi()),
                .allowDdcCi = changeSet->allowDdcCi.value_or(output->allowDdcCi()),
                .maxBitsPerColor = changeSet->maxBitsPerColor.value_or(output->maxBitsPerColor()),
                .edrPolicy = changeSet->edrPolicy.value_or(output->edrPolicy()),
            };
            *outputIt = SetupState{
                .outputIndex = *outputIndex,
                .position = changeSet->pos.value_or(output->geometry().topLeft()),
                .enabled = changeSet->enabled.value_or(output->isEnabled()),
                .priority = int(outputOrder.indexOf(output)),
                .replicationSource = changeSet->replicationSource.value_or(output->replicationSource()),
            };
        } else {
            QSize modeSize = output->desiredModeSize();
            if (modeSize.isEmpty()) {
                modeSize = output->currentMode()->size();
            }
            uint32_t refreshRate = output->desiredModeRefreshRate();
            if (refreshRate == 0) {
                refreshRate = output->currentMode()->refreshRate();
            }
            m_outputs[*outputIndex] = OutputState{
                .edidIdentifier = output->edid().identifier(),
                .connectorName = output->name(),
                .edidHash = output->edid().hash(),
                .mstPath = output->mstPath(),
                .mode = ModeData{
                    .size = modeSize,
                    .refreshRate = refreshRate,
                },
                .scale = output->scale(),
                .transform = output->transform(),
                .manualTransform = output->manualTransform(),
                .overscan = output->overscan(),
                .rgbRange = output->rgbRange(),
                .vrrPolicy = output->vrrPolicy(),
                .highDynamicRange = output->highDynamicRange(),
                .referenceLuminance = output->referenceLuminance(),
                .wideColorGamut = output->wideColorGamut(),
                .autoRotation = output->autoRotationPolicy(),
                .iccProfilePath = output->iccProfilePath(),
                .colorProfileSource = output->colorProfileSource(),
                .maxPeakBrightnessOverride = output->maxPeakBrightnessOverride(),
                .maxAverageBrightnessOverride = output->maxAverageBrightnessOverride(),
                .minBrightnessOverride = output->minBrightnessOverride(),
                .sdrGamutWideness = output->sdrGamutWideness(),
                .brightness = output->brightnessSetting(),
                .allowSdrSoftwareBrightness = output->allowSdrSoftwareBrightness(),
                .colorPowerTradeoff = output->colorPowerTradeoff(),
                .uuid = existingUuid,
                .detectedDdcCi = output->detectedDdcCi(),
                .allowDdcCi = output->allowDdcCi(),
                .maxBitsPerColor = output->maxBitsPerColor(),
                .edrPolicy = output->edrPolicy(),
            };
            *outputIt = SetupState{
                .outputIndex = *outputIndex,
                .position = output->geometry().topLeft(),
                .enabled = output->isEnabled(),
                .priority = int(outputOrder.indexOf(output)),
                .replicationSource = output->replicationSource(),
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
        const auto modeIt = std::find_if(modes.begin(), modes.end(), [&state](const auto &mode) {
            return state.mode
                && mode->size() == state.mode->size
                && mode->refreshRate() == state.mode->refreshRate;
        });
        std::optional<std::shared_ptr<OutputMode>> mode = modeIt == modes.end() ? std::nullopt : std::optional(*modeIt);
        if (!mode.has_value() || !*mode || ((*mode)->flags() & OutputMode::Flag::Removed)) {
            mode = chooseMode(output);
        }
        *ret.changeSet(output) = OutputChangeSet{
            .mode = mode,
            .desiredModeSize = state.mode.has_value() ? std::make_optional(state.mode->size) : std::nullopt,
            .desiredModeRefreshRate = state.mode.has_value() ? std::make_optional(state.mode->refreshRate) : std::nullopt,
            .enabled = setupState.enabled,
            .pos = setupState.position,
            .scale = state.scale,
            .transform = state.transform,
            .manualTransform = state.manualTransform,
            .overscan = state.overscan,
            .rgbRange = state.rgbRange,
            .vrrPolicy = state.vrrPolicy,
            .highDynamicRange = state.highDynamicRange,
            .referenceLuminance = state.referenceLuminance,
            .wideColorGamut = state.wideColorGamut,
            .autoRotationPolicy = state.autoRotation,
            .iccProfilePath = state.iccProfilePath,
            .iccProfile = state.iccProfilePath ? IccProfile::load(*state.iccProfilePath).value_or(nullptr) : nullptr,
            .maxPeakBrightnessOverride = state.maxPeakBrightnessOverride,
            .maxAverageBrightnessOverride = state.maxAverageBrightnessOverride,
            .minBrightnessOverride = state.minBrightnessOverride,
            .sdrGamutWideness = state.sdrGamutWideness,
            .colorProfileSource = state.colorProfileSource,
            .brightness = state.brightness,
            .allowSdrSoftwareBrightness = state.allowSdrSoftwareBrightness,
            .colorPowerTradeoff = state.colorPowerTradeoff,
            .uuid = state.uuid,
            .replicationSource = setupState.replicationSource,
            .detectedDdcCi = state.detectedDdcCi,
            .allowDdcCi = state.allowDdcCi,
            .maxBitsPerColor = state.maxBitsPerColor,
            .edrPolicy = state.edrPolicy,
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
    const auto kscreenConfig = KScreenIntegration::readOutputConfig(outputs, KScreenIntegration::connectedOutputsHash(outputs, isLidClosed));
    OutputConfiguration ret;
    QList<Output *> outputOrder;
    QPoint pos(0, 0);
    for (const auto output : outputs) {
        const auto kscreenChangeSetPtr = kscreenConfig ? kscreenConfig->first.constChangeSet(output) : nullptr;
        const auto kscreenChangeSet = kscreenChangeSetPtr ? *kscreenChangeSetPtr : OutputChangeSet{};

        const auto outputIndex = findOutput(output, outputs);
        const bool enable = kscreenChangeSet.enabled.value_or(!isLidClosed || !output->isInternal() || outputs.size() == 1);
        const OutputState existingData = outputIndex ? m_outputs[*outputIndex] : OutputState{};

        const auto modes = output->modes();
        const auto modeIt = std::find_if(modes.begin(), modes.end(), [&existingData](const auto &mode) {
            return existingData.mode
                && mode->size() == existingData.mode->size
                && mode->refreshRate() == existingData.mode->refreshRate;
        });
        const auto mode = modeIt == modes.end() ? kscreenChangeSet.mode.value_or(chooseMode(output)).lock() : *modeIt;

        const auto changeset = ret.changeSet(output);
        *changeset = {
            .mode = mode,
            .desiredModeSize = mode->size(),
            .desiredModeRefreshRate = mode->refreshRate(),
            .enabled = kscreenChangeSet.enabled.value_or(enable),
            .pos = pos,
            // kscreen scale is unreliable because it gets overwritten with the value 1 on Xorg,
            // and we don't know if it's from Xorg or the 5.27 Wayland session... so just ignore it
            .scale = existingData.scale.value_or(chooseScale(output, mode.get())),
            .transform = existingData.transform.value_or(kscreenChangeSet.transform.value_or(output->panelOrientation())),
            .manualTransform = existingData.manualTransform.value_or(kscreenChangeSet.transform.value_or(output->panelOrientation())),
            .overscan = existingData.overscan.value_or(kscreenChangeSet.overscan.value_or(0)),
            .rgbRange = existingData.rgbRange.value_or(kscreenChangeSet.rgbRange.value_or(Output::RgbRange::Automatic)),
            .vrrPolicy = existingData.vrrPolicy.value_or(kscreenChangeSet.vrrPolicy.value_or(VrrPolicy::Never)),
            .highDynamicRange = existingData.highDynamicRange.value_or(false),
            .referenceLuminance = existingData.referenceLuminance.value_or(std::clamp(output->maxAverageBrightness().value_or(200), 200.0, 500.0)),
            .wideColorGamut = existingData.wideColorGamut.value_or(false),
            .autoRotationPolicy = existingData.autoRotation.value_or(Output::AutoRotationPolicy::InTabletMode),
            .colorProfileSource = existingData.colorProfileSource.value_or(Output::ColorProfileSource::sRGB),
            .brightness = existingData.brightness.value_or(1.0),
            .allowSdrSoftwareBrightness = existingData.allowSdrSoftwareBrightness.value_or(output->brightnessDevice() == nullptr),
            .colorPowerTradeoff = existingData.colorPowerTradeoff.value_or(Output::ColorPowerTradeoff::PreferEfficiency),
            .uuid = existingData.uuid,
            .detectedDdcCi = existingData.detectedDdcCi.value_or(false),
            .allowDdcCi = existingData.allowDdcCi.value_or(!output->isDdcCiKnownBroken()),
            .maxBitsPerColor = existingData.maxBitsPerColor,
            .edrPolicy = existingData.edrPolicy.value_or(Output::EdrPolicy::Always),
        };
        if (enable) {
            const auto modeSize = changeset->transform->map(mode->size());
            pos.setX(std::ceil(pos.x() + modeSize.width() / *changeset->scale));
            outputOrder.push_back(output);
        }
    }
    if (kscreenConfig && kscreenConfig->second.size() == outputOrder.size()) {
        // make sure the old output order is consistent with the enablement states of the outputs
        const bool consistent = std::ranges::all_of(outputOrder, [&kscreenConfig](const auto output) {
            return kscreenConfig->second.contains(output);
        });
        if (consistent) {
            outputOrder = kscreenConfig->second;
        }
    }
    return std::make_pair(ret, outputOrder);
}

std::shared_ptr<OutputMode> OutputConfigurationStore::chooseMode(Output *output) const
{
    const auto findBiggestFastest = [](const auto &left, const auto &right) {
        const uint64_t leftPixels = left->size().width() * left->size().height();
        const uint64_t rightPixels = right->size().width() * right->size().height();
        if (leftPixels == rightPixels) {
            return left->refreshRate() < right->refreshRate();
        } else {
            return leftPixels < rightPixels;
        }
    };

    const auto modes = output->modes();
    auto notPotentiallyBroken = modes | std::ranges::views::filter([](const auto &mode) {
        // generated modes aren't guaranteed to work, so don't choose one as the default
        return !(mode->flags() & OutputMode::Flag::Generated)
            && !(mode->flags() & OutputMode::Flag::Removed);
    });
    if (notPotentiallyBroken.empty()) {
        // there's nothing more we can do
        return *std::ranges::max_element(modes, findBiggestFastest);
    }

    // 32:9 displays often advertise a lower resolution mode as preferred, special case them
    auto only32by9 = notPotentiallyBroken | std::ranges::views::filter([](const auto &mode) {
        const double aspectRatio = mode->size().width() / double(mode->size().height());
        return aspectRatio > 31 / 9.0 && aspectRatio < 33 / 9.0;
    });
    const auto best32By9 = std::ranges::max_element(only32by9, findBiggestFastest);
    if (best32By9 != only32by9.end()) {
        return *best32By9;
    }

    // try to figure out the native resolution; the biggest preferred mode usually has that
    auto preferredOnly = notPotentiallyBroken | std::ranges::views::filter([](const auto &mode) {
        return (mode->flags() & OutputMode::Flag::Preferred);
    });
    const auto nativeSize = std::ranges::max_element(preferredOnly, findBiggestFastest);
    if (nativeSize != preferredOnly.end() || output->edid().likelyNativeResolution()) {
        const auto size = nativeSize != preferredOnly.end() ? (*nativeSize)->size() : *output->edid().likelyNativeResolution();
        auto correctSize = notPotentiallyBroken | std::ranges::views::filter([size](const auto &mode) {
            return mode->size() == size;
        });
        // some high refresh rate displays advertise a 60Hz mode as preferred for compatibility reasons
        // ignore that and choose the highest possible refresh rate by default instead
        const auto highestRefresh = std::ranges::max_element(correctSize, [](const auto &n, const auto &nPlus1) {
            return n->refreshRate() < nPlus1->refreshRate();
        });
        // if the preferred mode size has a refresh rate that's too low for PCs,
        // allow falling back to a mode with lower resolution and a more usable refresh rate
        if (highestRefresh != correctSize.end() && (*highestRefresh)->refreshRate() >= 50000) {
            return *highestRefresh;
        }
    }

    // even if a higher resolution mode is available, try to pick a more usable refresh rate
    auto usableRefreshRates = notPotentiallyBroken | std::ranges::views::filter([](const auto &mode) {
        return mode->refreshRate() >= 50000;
    });
    const auto usable = std::ranges::max_element(usableRefreshRates, findBiggestFastest);
    if (usable != usableRefreshRates.end()) {
        return *usable;
    } else {
        return *std::ranges::max_element(notPotentiallyBroken, findBiggestFastest);
    }
}

double OutputConfigurationStore::chooseScale(Output *output, OutputMode *mode) const
{
    if (output->physicalSize().height() < 3 || output->physicalSize().width() < 3) {
        // A screen less than 3mm wide or tall doesn't make any sense; these are
        // all caused by the screen mis-reporting its size.
        return 1.0;
    }

    // The eye's ability to perceive detail diminishes with distance, so objects
    // that are closer can be smaller and their details remain equally
    // distinguishable. As a result, each device type has its own ideal physical
    // size of items on its screen based on how close the user's eyes are
    // expected to be from it on average, and its target DPI value needs to be
    // changed accordingly.
    //
    // The minSize specifies the minimum amount of logical pixels that must be available
    // after applying the chosen scale factor.
    double targetDpi;
    double minSize;
    if (output->isInternal()) {
        const bool hasLaptopLid = std::ranges::any_of(input()->devices(), [](const auto &device) {
            return device->isLidSwitch();
        });
        if (hasLaptopLid) {
            // laptop screens: usually closer to the face than desktop monitors
            targetDpi = 125;
            minSize = 800;
        } else {
            // phone screens: even closer than laptops
            targetDpi = 150;
            minSize = 360;
        }
    } else {
        // "normal" 1x scale desktop monitor dpi
        targetDpi = 96;
        minSize = 800;
    }

    const double dpiX = mode->size().width() / (output->physicalSize().width() / 25.4);
    const double maxScaleX = std::clamp(mode->size().width() / minSize, 1.0, 3.0);
    const double scaleX = std::clamp(dpiX / targetDpi, 1.0, maxScaleX);

    const double dpiY = mode->size().height() / (output->physicalSize().height() / 25.4);
    const double maxScaleY = std::clamp(mode->size().height() / minSize, 1.0, 3.0);
    const double scaleY = std::clamp(dpiY / targetDpi, 1.0, maxScaleY);

    const double scale = std::min(scaleX, scaleY);
    const double steps = 5;
    return std::round(100.0 * scale / steps) * steps / 100.0;
}

void OutputConfigurationStore::registerOutputs(const QList<Output *> &outputs)
{
    for (Output *output : outputs) {
        if (output->isNonDesktop() || output->isPlaceholder()) {
            continue;
        }
        auto index = findOutput(output, outputs);
        if (!index) {
            index = m_outputs.size();
            m_outputs.push_back(OutputState{});
        }
        auto &state = m_outputs[*index];
        state.edidIdentifier = output->edid().identifier();
        state.connectorName = output->name();
        state.edidHash = output->edid().hash();
        state.mstPath = output->mstPath();
        if (!state.uuid.has_value()) {
            state.uuid = QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces);
        }
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
        if (const auto it = data.find("edidHash"); it != data.end()) {
            if (const auto str = it->toString(); !str.isEmpty()) {
                state.edidHash = str;
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
                && data->edidHash == state.edidHash
                && data->mstPath == state.mstPath
                && data->connectorName == state.connectorName;
        });
        if (hasDuplicate) {
            qCWarning(KWIN_CORE) << "Duplicate output found in config for edidIdentifier:" << state.edidIdentifier << "; connectorName:" << state.connectorName << "; mstPath:" << state.mstPath;
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
            if (scale > 0 && scale <= 5) {
                state.scale = scale;
            }
        }
        if (const auto it = data.find("transform"); it != data.end()) {
            const auto str = it->toString();
            if (str == "Normal") {
                state.transform = state.manualTransform = OutputTransform::Kind::Normal;
            } else if (str == "Rotated90") {
                state.transform = state.manualTransform = OutputTransform::Kind::Rotate90;
            } else if (str == "Rotated180") {
                state.transform = state.manualTransform = OutputTransform::Kind::Rotate180;
            } else if (str == "Rotated270") {
                state.transform = state.manualTransform = OutputTransform::Kind::Rotate270;
            } else if (str == "Flipped") {
                state.transform = state.manualTransform = OutputTransform::Kind::FlipX;
            } else if (str == "Flipped90") {
                state.transform = state.manualTransform = OutputTransform::Kind::FlipX90;
            } else if (str == "Flipped180") {
                state.transform = state.manualTransform = OutputTransform::Kind::FlipX180;
            } else if (str == "Flipped270") {
                state.transform = state.manualTransform = OutputTransform::Kind::FlipX270;
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
                state.vrrPolicy = VrrPolicy::Never;
            } else if (str == "Automatic") {
                state.vrrPolicy = VrrPolicy::Automatic;
            } else if (str == "Always") {
                state.vrrPolicy = VrrPolicy::Always;
            }
        }
        if (const auto it = data.find("highDynamicRange"); it != data.end() && it->isBool()) {
            state.highDynamicRange = it->toBool();
        }
        if (const auto it = data.find("sdrBrightness"); it != data.end() && it->isDouble()) {
            state.referenceLuminance = it->toInt(200);
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
            if (*state.maxPeakBrightnessOverride < 50) {
                // clearly nonsense
                state.maxPeakBrightnessOverride.reset();
            }
        }
        if (const auto it = data.find("maxAverageBrightnessOverride"); it != data.end() && it->isDouble()) {
            state.maxAverageBrightnessOverride = it->toDouble();
            if (*state.maxAverageBrightnessOverride < 50) {
                // clearly nonsense
                state.maxAverageBrightnessOverride.reset();
            }
        }
        if (const auto it = data.find("minBrightnessOverride"); it != data.end() && it->isDouble()) {
            state.minBrightnessOverride = it->toDouble();
        }
        if (const auto it = data.find("sdrGamutWideness"); it != data.end() && it->isDouble()) {
            state.sdrGamutWideness = it->toDouble();
        }
        if (const auto it = data.find("colorProfileSource"); it != data.end()) {
            const auto str = it->toString();
            if (str == "sRGB") {
                state.colorProfileSource = Output::ColorProfileSource::sRGB;
            } else if (str == "ICC") {
                state.colorProfileSource = Output::ColorProfileSource::ICC;
            } else if (str == "EDID") {
                state.colorProfileSource = Output::ColorProfileSource::EDID;
            }
        } else {
            const bool icc = state.iccProfilePath && !state.iccProfilePath->isEmpty() && !state.highDynamicRange.value_or(false) && !state.wideColorGamut.value_or(false);
            if (icc) {
                state.colorProfileSource = Output::ColorProfileSource::ICC;
            } else {
                state.colorProfileSource = Output::ColorProfileSource::sRGB;
            }
        }
        if (const auto it = data.find("brightness"); it != data.end() && it->isDouble()) {
            state.brightness = std::clamp(it->toDouble(), 0.0, 1.0);
        }
        if (const auto it = data.find("allowSdrSoftwareBrightness"); it != data.end() && it->isBool()) {
            state.allowSdrSoftwareBrightness = it->toBool();
        }
        if (const auto it = data.find("colorPowerTradeoff"); it != data.end()) {
            const auto str = it->toString();
            if (str == "PreferEfficiency") {
                state.colorPowerTradeoff = Output::ColorPowerTradeoff::PreferEfficiency;
            } else if (str == "PreferAccuracy") {
                state.colorPowerTradeoff = Output::ColorPowerTradeoff::PreferAccuracy;
            }
        }
        if (const auto it = data.find("uuid"); it != data.end() && !it->toString().isEmpty()) {
            state.uuid = it->toString();
        }
        if (const auto it = data.find("detectedDdcCi"); it != data.end() && it->isBool()) {
            state.detectedDdcCi = it->toBool();
        }
        if (const auto it = data.find("allowDdcCi"); it != data.end() && it->isBool()) {
            state.allowDdcCi = it->toBool();
        }
        if (const auto it = data.find("maxBitsPerColor"); it != data.end()) {
            uint64_t bpc = it->toInteger(0);
            if (bpc >= 6 && bpc <= 16) {
                state.maxBitsPerColor = bpc;
            }
        }
        if (const auto it = data.find("edrPolicy"); it != data.end()) {
            const auto str = it->toString();
            if (str == "never") {
                state.edrPolicy = Output::EdrPolicy::Never;
            } else if (str == "always") {
                state.edrPolicy = Output::EdrPolicy::Always;
            }
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
            } else {
                fail = true;
                break;
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
            } else {
                state.priority = INT_MAX;
            }
            if (const auto it = outputData.find("replicationSource"); it != outputData.end()) {
                state.replicationSource = it->toString();
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
            // ensure that uuids are actually unique in the config
            const int count = std::ranges::count_if(outputDatas, [i, &outputDatas](const auto &data) {
                return data.has_value() && data->uuid == outputDatas[i]->uuid;
            });
            if (count > 1) {
                // a new uuid will be generated when the data gets used
                outputDatas[i]->uuid.reset();
            }
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
        if (!output.edidIdentifier.isEmpty()) {
            o["edidIdentifier"] = output.edidIdentifier;
        }
        if (!output.edidHash.isEmpty()) {
            o["edidHash"] = output.edidHash;
        }
        if (!output.connectorName.isEmpty()) {
            o["connectorName"] = output.connectorName;
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
        } else if (output.manualTransform == OutputTransform::Kind::Rotate90) {
            o["transform"] = "Rotated90";
        } else if (output.manualTransform == OutputTransform::Kind::Rotate180) {
            o["transform"] = "Rotated180";
        } else if (output.manualTransform == OutputTransform::Kind::Rotate270) {
            o["transform"] = "Rotated270";
        } else if (output.manualTransform == OutputTransform::Kind::FlipX) {
            o["transform"] = "Flipped";
        } else if (output.manualTransform == OutputTransform::Kind::FlipX90) {
            o["transform"] = "Flipped90";
        } else if (output.manualTransform == OutputTransform::Kind::FlipX180) {
            o["transform"] = "Flipped180";
        } else if (output.manualTransform == OutputTransform::Kind::FlipX270) {
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
        if (output.vrrPolicy == VrrPolicy::Never) {
            o["vrrPolicy"] = "Never";
        } else if (output.vrrPolicy == VrrPolicy::Automatic) {
            o["vrrPolicy"] = "Automatic";
        } else if (output.vrrPolicy == VrrPolicy::Always) {
            o["vrrPolicy"] = "Always";
        }
        if (output.highDynamicRange) {
            o["highDynamicRange"] = *output.highDynamicRange;
        }
        if (output.referenceLuminance) {
            o["sdrBrightness"] = int(*output.referenceLuminance);
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
        if (output.colorProfileSource) {
            switch (*output.colorProfileSource) {
            case Output::ColorProfileSource::sRGB:
                o["colorProfileSource"] = "sRGB";
                break;
            case Output::ColorProfileSource::ICC:
                o["colorProfileSource"] = "ICC";
                break;
            case Output::ColorProfileSource::EDID:
                o["colorProfileSource"] = "EDID";
                break;
            }
        }
        if (output.brightness) {
            o["brightness"] = *output.brightness;
        }
        if (output.allowSdrSoftwareBrightness) {
            o["allowSdrSoftwareBrightness"] = *output.allowSdrSoftwareBrightness;
        }
        if (output.colorPowerTradeoff) {
            switch (*output.colorPowerTradeoff) {
            case Output::ColorPowerTradeoff::PreferEfficiency:
                o["colorPowerTradeoff"] = "PreferEfficiency";
                break;
            case Output::ColorPowerTradeoff::PreferAccuracy:
                o["colorPowerTradeoff"] = "PreferAccuracy";
                break;
            }
        }
        if (output.uuid.has_value()) {
            o["uuid"] = *output.uuid;
        }
        if (output.detectedDdcCi) {
            o["detectedDdcCi"] = *output.detectedDdcCi;
        }
        if (output.allowDdcCi) {
            o["allowDdcCi"] = *output.allowDdcCi;
        }
        if (output.maxBitsPerColor.has_value()) {
            o["maxBitsPerColor"] = int(*output.maxBitsPerColor);
        }
        if (output.edrPolicy.has_value()) {
            switch (*output.edrPolicy) {
            case Output::EdrPolicy::Never:
                o["edrPolicy"] = "never";
                break;
            case Output::EdrPolicy::Always:
                o["edrPolicy"] = "always";
                break;
            }
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
            o["replicationSource"] = output.replicationSource;

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
    // if output is not on, disable auto-rotate
    if (internal->dpmsMode() != Output::DpmsMode::On) {
        return false;
    }
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
