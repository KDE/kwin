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

struct XcursorThemeClosure
{
    QMap<QByteArray, QVector<KXcursorSprite>> registry;
    int desiredSize;
};

static void load_callback(XcursorImages *images, void *data)
{
    XcursorThemeClosure *closure = static_cast<XcursorThemeClosure *>(data);
    QVector<KXcursorSprite> sprites;

    for (int i = 0; i < images->nimage; ++i) {
        const XcursorImage *nativeCursorImage = images->images[i];
        const qreal scale = std::max(qreal(1), qreal(nativeCursorImage->size) / closure->desiredSize);
        const QPoint hotspot(nativeCursorImage->xhot, nativeCursorImage->yhot);
        const std::chrono::milliseconds delay(nativeCursorImage->delay);

        QImage data(nativeCursorImage->width, nativeCursorImage->height, QImage::Format_ARGB32_Premultiplied);
        data.setDevicePixelRatio(scale);
        memcpy(data.bits(), nativeCursorImage->pixels, data.sizeInBytes());

        sprites.append(KXcursorSprite(data, hotspot / scale, delay));
    }

    if (!sprites.isEmpty()) {
        closure->registry.insert(images->name, sprites);
    }
    XcursorImagesDestroy(images);
}

KXcursorTheme::KXcursorTheme()
    : d(new KXcursorThemePrivate)
{
}

KXcursorTheme::KXcursorTheme(const QMap<QByteArray, QVector<KXcursorSprite>> &registry)
    : KXcursorTheme()
{
    d->registry = registry;
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
    // Xcursors don't support HiDPI natively so we fake it by scaling the desired cursor
    // size. The device pixel ratio argument acts only as a hint. The real scale factor
    // of every cursor sprite will be computed in the loading closure.
    XcursorThemeClosure closure;
    closure.desiredSize = size;
    xcursor_load_theme(themeName.toUtf8().constData(), size * dpr, load_callback, &closure);

    if (closure.registry.isEmpty()) {
        return KXcursorTheme();
    }

    return KXcursorTheme(closure.registry);
}

} // namespace KWin
