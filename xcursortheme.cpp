/*
 * Copyright (C) 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "xcursortheme.h"
#include "3rdparty/xcursor.h"

namespace KWin
{

KXcursorSprite::KXcursorSprite()
{
}

KXcursorSprite::KXcursorSprite(const QImage &data, const QPoint &hotspot,
                               const std::chrono::milliseconds &delay)
    : m_data(data)
    , m_hotspot(hotspot)
    , m_delay(delay)
{
}

QImage KXcursorSprite::data() const
{
    return m_data;
}

QPoint KXcursorSprite::hotspot() const
{
    return m_hotspot;
}

std::chrono::milliseconds KXcursorSprite::delay() const
{
    return m_delay;
}

static void load_callback(XcursorImages *images, void *data)
{
    QVector<KXcursorSprite> sprites;

    for (int i = 0; i < images->nimage; ++i) {
        const XcursorImage *nativeCursorImage = images->images[i];
        const QPoint hotspot(nativeCursorImage->xhot, nativeCursorImage->yhot);
        const std::chrono::milliseconds delay(nativeCursorImage->delay);

        QImage data(nativeCursorImage->width, nativeCursorImage->height, QImage::Format_ARGB32);
        memcpy(data.bits(), nativeCursorImage->pixels, data.sizeInBytes());

        sprites.append(KXcursorSprite(data, hotspot, delay));
    }

    auto cursorRegistry = static_cast<QMap<QByteArray, QVector<KXcursorSprite>> *>(data);
    cursorRegistry->insert(images->name, sprites);

    XcursorImagesDestroy(images);
}

qreal KXcursorTheme::devicePixelRatio() const
{
    return m_devicePixelRatio;
}

bool KXcursorTheme::isEmpty() const
{
    return m_cursorRegistry.isEmpty();
}

QVector<KXcursorSprite> KXcursorTheme::shape(const QByteArray &name) const
{
    return m_cursorRegistry.value(name);
}

KXcursorTheme KXcursorTheme::fromTheme(const QString &themeName, int size, qreal dpr)
{
    KXcursorTheme theme;
    theme.m_devicePixelRatio = dpr;

    const QByteArray nativeThemeName = themeName.toUtf8();
    xcursor_load_theme(nativeThemeName, size * dpr, load_callback, &theme.m_cursorRegistry);

    return theme;
}

} // namespace KWin
