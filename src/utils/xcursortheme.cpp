/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "xcursortheme.h"
#include "3rdparty/xcursor.h"

#include <KConfig>
#include <KConfigGroup>

#include <QDir>
#include <QFile>
#include <QSet>
#include <QSharedData>
#include <QStack>
#include <QStandardPaths>

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
    void load(const QString &themeName, int size, qreal devicePixelRatio);
    void loadCursors(const QString &packagePath, int size, qreal devicePixelRatio);

    QHash<QByteArray, QVector<KXcursorSprite>> registry;
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

static QVector<KXcursorSprite> loadCursor(const QString &filePath, int desiredSize, qreal devicePixelRatio)
{
    XcursorImages *images = XcursorFileLoadImages(QFile::encodeName(filePath), desiredSize * devicePixelRatio);
    if (!images) {
        return {};
    }

    QVector<KXcursorSprite> sprites;
    for (int i = 0; i < images->nimage; ++i) {
        const XcursorImage *nativeCursorImage = images->images[i];
        const qreal scale = std::max(qreal(1), qreal(nativeCursorImage->size) / desiredSize);
        const QPoint hotspot(nativeCursorImage->xhot, nativeCursorImage->yhot);
        const std::chrono::milliseconds delay(nativeCursorImage->delay);

        QImage data(nativeCursorImage->width, nativeCursorImage->height, QImage::Format_ARGB32_Premultiplied);
        data.setDevicePixelRatio(scale);
        memcpy(data.bits(), nativeCursorImage->pixels, data.sizeInBytes());

        sprites.append(KXcursorSprite(data, hotspot / scale, delay));
    }

    XcursorImagesDestroy(images);
    return sprites;
}

void KXcursorThemePrivate::loadCursors(const QString &packagePath, int size, qreal devicePixelRatio)
{
    const QDir dir(packagePath);
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    std::partition(entries.begin(), entries.end(), [](const QFileInfo &fileInfo) {
        return !fileInfo.isSymLink();
    });

    for (const QFileInfo &entry : std::as_const(entries)) {
        const QByteArray shape = QFile::encodeName(entry.fileName());
        if (registry.contains(shape)) {
            continue;
        }
        if (entry.isSymLink()) {
            const QFileInfo symLinkInfo(entry.symLinkTarget());
            if (symLinkInfo.absolutePath() == entry.absolutePath()) {
                const auto sprites = registry.value(QFile::encodeName(symLinkInfo.fileName()));
                if (!sprites.isEmpty()) {
                    registry.insert(shape, sprites);
                    continue;
                }
            }
        }
        const QVector<KXcursorSprite> sprites = loadCursor(entry.absoluteFilePath(), size, devicePixelRatio);
        if (!sprites.isEmpty()) {
            registry.insert(shape, sprites);
        }
    }
}

static QStringList searchPaths()
{
    static QStringList paths;
    if (paths.isEmpty()) {
        if (const QString env = qEnvironmentVariable("XCURSOR_PATH"); !env.isEmpty()) {
            paths.append(env.split(':', Qt::SkipEmptyParts));
        } else {
            const QString home = QDir::homePath();
            if (!home.isEmpty()) {
                paths.append(home + QLatin1String("/.icons"));
            }
            const QStringList dataDirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
            for (const QString &dataDir : dataDirs) {
                paths.append(dataDir + QLatin1String("/icons"));
            }
        }
    }
    return paths;
}

void KXcursorThemePrivate::load(const QString &themeName, int size, qreal devicePixelRatio)
{
    const QStringList paths = searchPaths();

    QStack<QString> stack;
    QSet<QString> loaded;

    stack.push(themeName);

    while (!stack.isEmpty()) {
        const QString themeName = stack.pop();
        if (loaded.contains(themeName)) {
            continue;
        }

        QStringList inherits;

        for (const QString &path : paths) {
            const QDir dir(path + QLatin1Char('/') + themeName);
            if (!dir.exists()) {
                continue;
            }
            loadCursors(dir.filePath(QStringLiteral("cursors")), size, devicePixelRatio);
            if (inherits.isEmpty()) {
                const KConfig config(dir.filePath(QStringLiteral("index.theme")), KConfig::NoGlobals);
                inherits << KConfigGroup(&config, "Icon Theme").readEntry("Inherits", QStringList());
            }
        }

        loaded.insert(themeName);
        for (auto it = inherits.crbegin(); it != inherits.crend(); ++it) {
            stack.push(*it);
        }
    }
}

KXcursorTheme::KXcursorTheme()
    : d(new KXcursorThemePrivate)
{
}

KXcursorTheme::KXcursorTheme(const QString &themeName, int size, qreal devicePixelRatio)
    : d(new KXcursorThemePrivate)
{
    d->load(themeName, size, devicePixelRatio);
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

bool KXcursorTheme::operator==(const KXcursorTheme &other)
{
    return d == other.d;
}

bool KXcursorTheme::operator!=(const KXcursorTheme &other)
{
    return !(*this == other);
}

bool KXcursorTheme::isEmpty() const
{
    return d->registry.isEmpty();
}

QVector<KXcursorSprite> KXcursorTheme::shape(const QByteArray &name) const
{
    return d->registry.value(name);
}

} // namespace KWin
