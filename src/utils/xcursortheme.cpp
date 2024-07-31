/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/xcursortheme.h"
#include "utils/svgcursorreader.h"
#include "utils/xcursorreader.h"

#include <KConfig>
#include <KConfigGroup>
#include <KShell>

#include <QDir>
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

struct KXcursorThemeXEntryInfo
{
    QString path;
};

struct KXcursorThemeSvgEntryInfo
{
    QString path;
};

using KXcursorThemeEntryInfo = std::variant<KXcursorThemeXEntryInfo,
                                            KXcursorThemeSvgEntryInfo>;

class KXcursorThemeEntry
{
public:
    explicit KXcursorThemeEntry(const KXcursorThemeEntryInfo &info);

    void load(int size, qreal devicePixelRatio);

    KXcursorThemeEntryInfo info;
    QList<KXcursorSprite> sprites;
};

class KXcursorThemePrivate : public QSharedData
{
public:
    KXcursorThemePrivate();
    KXcursorThemePrivate(const QString &themeName, int size, qreal devicePixelRatio);

    void discover(const QStringList &searchPaths);
    void discoverXCursors(const QString &packagePath);
    void discoverSvgCursors(const QString &packagePath);

    QString name;
    int size = 0;
    qreal devicePixelRatio = 0;

    QHash<QByteArray, std::shared_ptr<KXcursorThemeEntry>> registry;
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

KXcursorThemePrivate::KXcursorThemePrivate()
{
}

KXcursorThemePrivate::KXcursorThemePrivate(const QString &themeName, int size, qreal devicePixelRatio)
    : name(themeName)
    , size(size)
    , devicePixelRatio(devicePixelRatio)
{
}

KXcursorThemeEntry::KXcursorThemeEntry(const KXcursorThemeEntryInfo &info)
    : info(info)
{
}

void KXcursorThemeEntry::load(int size, qreal devicePixelRatio)
{
    if (!sprites.isEmpty()) {
        return;
    }

    if (const auto raster = std::get_if<KXcursorThemeXEntryInfo>(&info)) {
        sprites = XCursorReader::load(raster->path, size, devicePixelRatio);
    } else if (const auto svg = std::get_if<KXcursorThemeSvgEntryInfo>(&info)) {
        sprites = SvgCursorReader::load(svg->path, size, devicePixelRatio);
    }
}

void KXcursorThemePrivate::discoverXCursors(const QString &packagePath)
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
                if (auto alias = registry.value(QFile::encodeName(symLinkInfo.fileName()))) {
                    registry.insert(shape, alias);
                    continue;
                }
            }
        }
        registry.insert(shape, std::make_shared<KXcursorThemeEntry>(KXcursorThemeXEntryInfo{
            .path = entry.absoluteFilePath(),
        }));
    }
}

void KXcursorThemePrivate::discoverSvgCursors(const QString &packagePath)
{
    const QDir dir(packagePath);
    QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
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
                if (auto alias = registry.value(QFile::encodeName(symLinkInfo.fileName()))) {
                    registry.insert(shape, alias);
                    continue;
                }
            }
        }
        registry.insert(shape, std::make_shared<KXcursorThemeEntry>(KXcursorThemeSvgEntryInfo{
            .path = entry.absoluteFilePath(),
        }));
    }
}

static QStringList defaultSearchPaths()
{
    static QStringList paths;
    if (paths.isEmpty()) {
        if (const QString env = qEnvironmentVariable("XCURSOR_PATH"); !env.isEmpty()) {
            const QStringList rawPaths = env.split(':', Qt::SkipEmptyParts);
            for (const QString &rawPath : rawPaths) {
                paths.append(KShell::tildeExpand(rawPath));
            }
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

void KXcursorThemePrivate::discover(const QStringList &searchPaths)
{
    const QStringList paths = !searchPaths.isEmpty() ? searchPaths : defaultSearchPaths();

    QStack<QString> stack;
    QSet<QString> loaded;

    stack.push(name);

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
            if (const QDir package = dir.filePath(QLatin1String("cursors_scalable")); package.exists()) {
                discoverSvgCursors(package.path());
            } else if (const QDir package = dir.filePath(QLatin1String("cursors")); package.exists()) {
                discoverXCursors(package.path());
            }
            if (inherits.isEmpty()) {
                const KConfig config(dir.filePath(QStringLiteral("index.theme")), KConfig::NoGlobals);
                inherits << KConfigGroup(&config, QStringLiteral("Icon Theme")).readEntry("Inherits", QStringList());
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

KXcursorTheme::KXcursorTheme(const QString &themeName, int size, qreal devicePixelRatio, const QStringList &searchPaths)
    : d(new KXcursorThemePrivate(themeName, size, devicePixelRatio))
{
    d->discover(searchPaths);
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

QString KXcursorTheme::name() const
{
    return d->name;
}

int KXcursorTheme::size() const
{
    return d->size;
}

qreal KXcursorTheme::devicePixelRatio() const
{
    return d->devicePixelRatio;
}

bool KXcursorTheme::isEmpty() const
{
    return d->registry.isEmpty();
}

QList<KXcursorSprite> KXcursorTheme::shape(const QByteArray &name) const
{
    if (auto entry = d->registry.value(name)) {
        entry->load(d->size, d->devicePixelRatio);
        return entry->sprites;
    }
    return QList<KXcursorSprite>();
}

} // namespace KWin
