/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/svgcursorreader.h"
#include "utils/common.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSvgRenderer>

namespace KWin
{

struct SvgCursorMetaDataEntry
{
    static std::optional<SvgCursorMetaDataEntry> parse(const QJsonObject &object);

    QString fileName;
    QPointF hotspot;
    std::chrono::milliseconds delay;
};

std::optional<SvgCursorMetaDataEntry> SvgCursorMetaDataEntry::parse(const QJsonObject &object)
{
    const QJsonValue fileName = object.value(QLatin1String("filename"));
    if (!fileName.isString()) {
        return std::nullopt;
    }

    const QJsonValue hotspotX = object.value(QLatin1String("hotspot_x"));
    if (!hotspotX.isDouble()) {
        return std::nullopt;
    }

    const QJsonValue hotspotY = object.value(QLatin1String("hotspot_y"));
    if (!hotspotY.isDouble()) {
        return std::nullopt;
    }

    const QJsonValue frametime = object.value(QLatin1String("frametime"));

    return SvgCursorMetaDataEntry{
        .fileName = fileName.toString(),
        .hotspot = QPointF(hotspotX.toDouble(), hotspotY.toDouble()),
        .delay = std::chrono::milliseconds(frametime.toInt()),
    };
}

struct SvgCursorMetaData
{
    static std::optional<SvgCursorMetaData> parse(const QString &filePath);

    QList<SvgCursorMetaDataEntry> entries;
};

std::optional<SvgCursorMetaData> SvgCursorMetaData::parse(const QString &filePath)
{
    QFile metaDataFile(filePath);
    if (!metaDataFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }

    QJsonParseError jsonParseError;
    const QJsonDocument document = QJsonDocument::fromJson(metaDataFile.readAll(), &jsonParseError);
    if (jsonParseError.error) {
        return std::nullopt;
    }

    QList<SvgCursorMetaDataEntry> entries;
    if (document.isObject()) {
        if (const auto entry = SvgCursorMetaDataEntry::parse(document.object())) {
            entries.append(entry.value());
        } else {
            return std::nullopt;
        }
    } else if (document.isArray()) {
        const QJsonArray array = document.array();
        for (int i = 0; i < array.size(); ++i) {
            const QJsonValue element = array.at(i);
            if (!element.isObject()) {
                return std::nullopt;
            }
            if (const auto entry = SvgCursorMetaDataEntry::parse(element.toObject())) {
                entries.append(entry.value());
            } else {
                return std::nullopt;
            }
        }
    } else {
        return std::nullopt;
    }

    return SvgCursorMetaData{
        .entries = entries,
    };
}

QList<KXcursorSprite> SvgCursorReader::load(const QString &containerPath, int desiredSize, qreal devicePixelRatio)
{
    const QDir containerDir(containerPath);

    const QString metadataFilePath = containerDir.filePath(QStringLiteral("metadata.json"));
    const auto metadata = SvgCursorMetaData::parse(metadataFilePath);
    if (!metadata.has_value()) {
        qCWarning(KWIN_CORE) << "Failed to parse" << metadataFilePath;
        return {};
    }

    const qreal scale = desiredSize / 24.0;

    QList<KXcursorSprite> sprites;
    for (const SvgCursorMetaDataEntry &entry : metadata->entries) {
        const QString filePath = containerDir.filePath(entry.fileName);

        QSvgRenderer renderer(filePath);
        if (!renderer.isValid()) {
            qCWarning(KWIN_CORE) << "Failed to render" << filePath;
            return {};
        }

        const QRect bounds(QPoint(0, 0), renderer.defaultSize() * scale);
        QImage image(bounds.size() * devicePixelRatio, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        image.setDevicePixelRatio(devicePixelRatio);

        QPainter painter(&image);
        renderer.render(&painter, bounds);
        painter.end();

        sprites.append(KXcursorSprite(image, (entry.hotspot * scale).toPoint(), entry.delay));
    }

    return sprites;
}

} // namespace KWin
