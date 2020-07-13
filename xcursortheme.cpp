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

#include <QMap>
#include <QSharedData>

namespace KWin
{

class KXcursorSpritePrivate : public QSharedData
{
public:
    QImage data;
    QPoint hotspot;
    std::chrono::milliseconds delay;
};

class KXcursorThemePrivate : public QSharedData
{
public:
    QMap<QByteArray, QVector<KXcursorSprite>> registry;
    qreal devicePixelRatio = 1;
};

KXcursorSprite::KXcursorSprite()
    : d(new KXcursorSpritePrivate)
{
}

KXcursorSprite::KXcursorSprite(const KXcursorSprite &other)
    : d(other.d)
{
}

KXcursorSprite::~KXcursorSprite()
{
}

KXcursorSprite &KXcursorSprite::operator=(const KXcursorSprite &other)
{
    d = other.d;
    return *this;
}

KXcursorSprite::KXcursorSprite(const QImage &data, const QPoint &hotspot,
                               const std::chrono::milliseconds &delay)
    : d(new KXcursorSpritePrivate)
{
    d->data = data;
    d->hotspot = hotspot;
    d->delay = delay;
}

QImage KXcursorSprite::data() const
{
    return d->data;
}

QPoint KXcursorSprite::hotspot() const
{
    return d->hotspot;
}

std::chrono::milliseconds KXcursorSprite::delay() const
{
    return d->delay;
}

static void load_callback(XcursorImages *images, void *data)
{
    KXcursorThemePrivate *themePrivate = static_cast<KXcursorThemePrivate *>(data);
    QVector<KXcursorSprite> sprites;

    for (int i = 0; i < images->nimage; ++i) {
        const XcursorImage *nativeCursorImage = images->images[i];
        const QPoint hotspot(nativeCursorImage->xhot, nativeCursorImage->yhot);
        const std::chrono::milliseconds delay(nativeCursorImage->delay);

        QImage data(nativeCursorImage->width, nativeCursorImage->height, QImage::Format_ARGB32);
        memcpy(data.bits(), nativeCursorImage->pixels, data.sizeInBytes());

        sprites.append(KXcursorSprite(data, hotspot, delay));
    }

    themePrivate->registry.insert(images->name, sprites);
    XcursorImagesDestroy(images);
}

KXcursorTheme::KXcursorTheme()
    : d(new KXcursorThemePrivate)
{
}

KXcursorTheme::KXcursorTheme(const KXcursorTheme &other)
    : d(other.d)
{
}

KXcursorTheme::~KXcursorTheme()
{
}

KXcursorTheme &KXcursorTheme::operator=(const KXcursorTheme &other)
{
    d = other.d;
    return *this;
}

qreal KXcursorTheme::devicePixelRatio() const
{
    return d->devicePixelRatio;
}

bool KXcursorTheme::isEmpty() const
{
    return d->registry.isEmpty();
}

QVector<KXcursorSprite> KXcursorTheme::shape(const QByteArray &name) const
{
    return d->registry.value(name);
}

KXcursorTheme KXcursorTheme::fromTheme(const QString &themeName, int size, qreal dpr)
{
    KXcursorTheme theme;
    KXcursorThemePrivate *themePrivate = theme.d;
    themePrivate->devicePixelRatio = dpr;

    const QByteArray nativeThemeName = themeName.toUtf8();
    xcursor_load_theme(nativeThemeName, size * dpr, load_callback, themePrivate);

    return theme;
}

} // namespace KWin
