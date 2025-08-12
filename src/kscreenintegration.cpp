/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kscreenintegration.h"
#include "utils/common.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace KWin
{
namespace KScreenIntegration
{
/// See KScreen::LogicalOutput::hashMd5
static QString outputHash(LogicalOutput *output)
{
    if (!output->edid().hash().isEmpty()) {
        return output->edid().hash();
    } else {
        return output->name();
    }
}

/// See KScreen::Config::connectedOutputsHash in libkscreen
QString connectedOutputsHash(const QList<LogicalOutput *> &outputs, bool isLidClosed)
{
    QStringList hashedOutputs;
    hashedOutputs.reserve(outputs.count());
    for (auto output : std::as_const(outputs)) {
        if (output->isPlaceholder() || output->isNonDesktop()) {
            continue;
        }
        if (output->isInternal() && isLidClosed) {
            continue;
        }
        hashedOutputs << outputHash(output);
    }
    std::sort(hashedOutputs.begin(), hashedOutputs.end());
    const auto hash = QCryptographicHash::hash(hashedOutputs.join(QString()).toLatin1(), QCryptographicHash::Md5);
    return QString::fromLatin1(hash.toHex());
}

static QHash<LogicalOutput *, QJsonObject> outputsConfig(const QList<LogicalOutput *> &outputs, const QString &hash)
{
    const QString kscreenJsonPath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kscreen/") % hash);
    if (kscreenJsonPath.isEmpty()) {
        return {};
    }

    QFile f(kscreenJsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        qCWarning(KWIN_CORE) << "Could not open file" << kscreenJsonPath;
        return {};
    }

    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qCWarning(KWIN_CORE) << "Failed to parse" << kscreenJsonPath << error.errorString();
        return {};
    }

    QHash<LogicalOutput *, bool> duplicate;
    QHash<LogicalOutput *, QString> outputHashes;
    for (LogicalOutput *output : outputs) {
        const QString hash = outputHash(output);
        const auto it = std::find_if(outputHashes.cbegin(), outputHashes.cend(), [hash](const auto &value) {
            return value == hash;
        });
        if (it == outputHashes.cend()) {
            duplicate[output] = false;
        } else {
            duplicate[output] = true;
            duplicate[it.key()] = true;
        }
        outputHashes[output] = hash;
    }

    QHash<LogicalOutput *, QJsonObject> ret;
    const auto outputsJson = doc.array();
    for (const auto &outputJson : outputsJson) {
        const auto outputObject = outputJson.toObject();
        const auto id = outputObject[QLatin1String("id")];
        const auto output = std::find_if(outputs.begin(), outputs.end(), [&duplicate, &id, &outputObject](LogicalOutput *output) {
            if (outputHash(output) != id.toString()) {
                return false;
            }
            if (duplicate[output]) {
                // can't distinguish between outputs by hash alone, need to look at connector names
                const auto metadata = outputObject[QLatin1String("metadata")];
                const auto outputName = metadata[QLatin1String("name")].toString();
                return outputName == output->name();
            } else {
                return true;
            }
        });
        if (output != outputs.end()) {
            ret[*output] = outputObject;
        }
    }
    return ret;
}

static std::optional<QJsonObject> globalOutputConfig(LogicalOutput *output)
{
    const QString kscreenPath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kscreen/"));
    if (kscreenPath.isEmpty()) {
        return std::nullopt;
    }
    const auto hash = outputHash(output);
    // use connector specific data if available, unspecific data if not
    QFile f(kscreenPath % hash % output->name());
    if (!f.open(QIODevice::ReadOnly)) {
        f.setFileName(kscreenPath % hash);
        if (!f.open(QIODevice::ReadOnly)) {
            qCWarning(KWIN_CORE) << "Could not open file" << f.fileName();
            return std::nullopt;
        }
    }

    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qCWarning(KWIN_CORE) << "Failed to parse" << f.fileName() << error.errorString();
        return std::nullopt;
    }
    return doc.object();
}

/// See KScreen::LogicalOutput::Rotation
enum Rotation {
    None = 1,
    Left = 2,
    Inverted = 4,
    Right = 8,
};

OutputTransform toKWinTransform(int rotation)
{
    switch (Rotation(rotation)) {
    case None:
        return OutputTransform::Normal;
    case Left:
        return OutputTransform::Rotate90;
    case Inverted:
        return OutputTransform::Rotate180;
    case Right:
        return OutputTransform::Rotate270;
    default:
        Q_UNREACHABLE();
    }
}

std::shared_ptr<OutputMode> parseMode(LogicalOutput *output, const QJsonObject &modeInfo)
{
    const QJsonObject size = modeInfo["size"].toObject();
    const QSize modeSize = QSize(size["width"].toInt(), size["height"].toInt());
    const uint32_t refreshRate = std::round(modeInfo["refresh"].toDouble() * 1000);

    const auto modes = output->modes();
    auto it = std::find_if(modes.begin(), modes.end(), [&modeSize, &refreshRate](const auto &mode) {
        return mode->size() == modeSize && mode->refreshRate() == refreshRate;
    });
    return (it != modes.end()) ? *it : nullptr;
}

std::optional<std::pair<OutputConfiguration, QList<LogicalOutput *>>> readOutputConfig(const QList<LogicalOutput *> &outputs, const QString &hash)
{
    const auto outputsInfo = outputsConfig(outputs, hash);
    if (outputsInfo.isEmpty()) {
        return std::nullopt;
    }
    std::vector<std::pair<uint32_t, LogicalOutput *>> outputOrder;
    OutputConfiguration cfg;
    // default position goes from left to right
    QPoint pos(0, 0);
    for (const auto &output : std::as_const(outputs)) {
        if (output->isPlaceholder() || output->isNonDesktop()) {
            continue;
        }
        auto props = cfg.changeSet(output);
        const QJsonObject outputInfo = outputsInfo[output];
        const auto globalOutputInfo = globalOutputConfig(output);
        qCDebug(KWIN_CORE) << "Reading output configuration for " << output;
        if (!outputInfo.isEmpty() || globalOutputInfo.has_value()) {
            // settings that are per output setup:
            props->enabled = outputInfo["enabled"].toBool(true);
            if (outputInfo["primary"].toBool()) {
                outputOrder.push_back(std::make_pair(1, output));
                if (!props->enabled) {
                    qCWarning(KWIN_CORE) << "KScreen config would disable the primary output!";
                    return std::nullopt;
                }
            } else if (int prio = outputInfo["priority"].toInt(); prio > 0) {
                outputOrder.push_back(std::make_pair(prio, output));
                if (!props->enabled) {
                    qCWarning(KWIN_CORE) << "KScreen config would disable an output with priority!";
                    return std::nullopt;
                }
            } else {
                outputOrder.push_back(std::make_pair(0, output));
            }
            if (const QJsonObject pos = outputInfo["pos"].toObject(); !pos.isEmpty()) {
                props->pos = QPoint(pos["x"].toInt(), pos["y"].toInt());
            }

            // settings that are independent of per output setups:
            const auto &globalInfo = globalOutputInfo ? globalOutputInfo.value() : outputInfo;
            if (const QJsonValue scale = globalInfo["scale"]; !scale.isUndefined()) {
                props->scale = scale.toDouble(1.);
            }
            if (const QJsonValue rotation = globalInfo["rotation"]; !rotation.isUndefined()) {
                props->transform = KScreenIntegration::toKWinTransform(rotation.toInt());
                props->manualTransform = props->transform;
            }
            if (const QJsonValue overscan = globalInfo["overscan"]; !overscan.isUndefined()) {
                props->overscan = globalInfo["overscan"].toInt();
            }
            if (const QJsonValue vrrpolicy = globalInfo["vrrpolicy"]; !vrrpolicy.isUndefined()) {
                props->vrrPolicy = static_cast<VrrPolicy>(vrrpolicy.toInt());
            }
            if (const QJsonValue rgbrange = globalInfo["rgbrange"]; !rgbrange.isUndefined()) {
                props->rgbRange = static_cast<LogicalOutput::RgbRange>(rgbrange.toInt());
            }

            if (const QJsonObject modeInfo = globalInfo["mode"].toObject(); !modeInfo.isEmpty()) {
                if (auto mode = KScreenIntegration::parseMode(output, modeInfo)) {
                    props->mode = mode;
                }
            }
        } else {
            props->enabled = true;
            props->pos = pos;
            props->transform = output->panelOrientation();
            outputOrder.push_back(std::make_pair(0, output));
        }
        pos.setX(pos.x() + output->geometry().width());
    }

    bool allDisabled = std::all_of(outputs.begin(), outputs.end(), [&cfg](const auto &output) {
        return !cfg.changeSet(output)->enabled.value_or(output->isEnabled());
    });
    if (allDisabled) {
        qCWarning(KWIN_CORE) << "KScreen config would disable all outputs!";
        return std::nullopt;
    }
    std::erase_if(outputOrder, [&cfg](const auto &pair) {
        return !cfg.constChangeSet(pair.second)->enabled.value_or(pair.second->isEnabled());
    });
    std::sort(outputOrder.begin(), outputOrder.end(), [](const auto &left, const auto &right) {
        if (left.first != right.first) {
            // All the outputs marked with prio 0 should be at the end of the list
            if (left.first == 0) {
                return false;
            }
            if (right.first == 0) {
                return true;
            }

            return left.first < right.first;
        }
        // sort alphabetically as a fallback
        return left.second->name() < right.second->name();
    });

    QList<LogicalOutput *> order;
    order.reserve(outputOrder.size());
    std::transform(outputOrder.begin(), outputOrder.end(), std::back_inserter(order), [](const auto &pair) {
        return pair.second;
    });
    return std::make_pair(cfg, order);
}
}
}
