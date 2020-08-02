/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
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

        sprites.append(KXcursorSprite(data, hotspot / themePrivate->devicePixelRatio, delay));
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
