/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/xcursorreader.h"
#include "3rdparty/xcursor.h"

#include <QFile>

namespace KWin
{

QList<CursorSprite> XCursorReader::load(const QString &filePath, int desiredSize, qreal devicePixelRatio)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly)) {
        return {};
    }

    XcursorFile reader {
        .closure = &file,
        .read = [](XcursorFile *file, uint8_t *buffer, int len) -> int {
            QFile *device = static_cast<QFile *>(file->closure);
            return device->read(reinterpret_cast<char *>(buffer), len);
        },
        .skip = [](XcursorFile *file, long offset) -> XcursorBool {
            QFile *device = static_cast<QFile *>(file->closure);
            return device->skip(offset) != -1;
        },
        .seek = [](XcursorFile *file, long offset) -> XcursorBool {
            QFile *device = static_cast<QFile *>(file->closure);
            return device->seek(offset);
        },
    };

    XcursorImages *images = XcursorXcFileLoadImages(&reader, desiredSize * devicePixelRatio);
    if (!images) {
        return {};
    }

    QList<CursorSprite> sprites;
    for (int i = 0; i < images->nimage; ++i) {
        const XcursorImage *nativeCursorImage = images->images[i];
        const qreal scale = std::max(qreal(1), qreal(nativeCursorImage->size) / desiredSize);
        const QPointF hotspot(nativeCursorImage->xhot, nativeCursorImage->yhot);
        const std::chrono::milliseconds delay(nativeCursorImage->delay);

        QImage data(nativeCursorImage->width, nativeCursorImage->height, QImage::Format_ARGB32_Premultiplied);
        data.setDevicePixelRatio(scale);
        memcpy(data.bits(), nativeCursorImage->pixels, data.sizeInBytes());

        sprites.append(CursorSprite(data, hotspot / scale, delay));
    }

    XcursorImagesDestroy(images);
    return sprites;
}

} // namespace KWin
