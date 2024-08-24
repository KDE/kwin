/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/cursortheme.h"
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

class CursorSpritePrivate : public QSharedData
{
public:
    QImage data;
    QPointF hotspot;
    std::chrono::milliseconds delay;
};

struct CursorThemeXEntryInfo
{
    QString path;
};

struct CursorThemeSvgEntryInfo
{
    QString path;
};

using CursorThemeEntryInfo = std::variant<CursorThemeXEntryInfo,
                                          CursorThemeSvgEntryInfo>;

class CursorThemeEntry
{
public:
    explicit CursorThemeEntry(const CursorThemeEntryInfo &info);

    void load(int size, qreal devicePixelRatio);

    CursorThemeEntryInfo info;
    QList<CursorSprite> sprites;
};

class CursorThemePrivate : public QSharedData
{
public:
    CursorThemePrivate();
    CursorThemePrivate(const QString &themeName, int size, qreal devicePixelRatio);

    void discover(const QStringList &searchPaths);
    void discoverXCursors(const QString &packagePath);
    void discoverSvgCursors(const QString &packagePath);

    QString name;
    int size = 0;
    qreal devicePixelRatio = 0;

    QHash<QByteArray, std::shared_ptr<CursorThemeEntry>> registry;
};

CursorSprite::CursorSprite()
    : d(new CursorSpritePrivate)
{
}

CursorSprite::CursorSprite(const CursorSprite &other)
    : d(other.d)
{
}

CursorSprite::~CursorSprite()
{
}

CursorSprite &CursorSprite::operator=(const CursorSprite &other)
{
    d = other.d;
    return *this;
}

CursorSprite::CursorSprite(const QImage &data, const QPointF &hotspot, const std::chrono::milliseconds &delay)
    : d(new CursorSpritePrivate)
{
    d->data = data;
    d->hotspot = hotspot;
    d->delay = delay;
}

QImage CursorSprite::data() const
{
    return d->data;
}

QPointF CursorSprite::hotspot() const
{
    return d->hotspot;
}

std::chrono::milliseconds CursorSprite::delay() const
{
    return d->delay;
}

CursorThemePrivate::CursorThemePrivate()
{
}

CursorThemePrivate::CursorThemePrivate(const QString &themeName, int size, qreal devicePixelRatio)
    : name(themeName)
    , size(size)
    , devicePixelRatio(devicePixelRatio)
{
}

CursorThemeEntry::CursorThemeEntry(const CursorThemeEntryInfo &info)
    : info(info)
{
}

void CursorThemeEntry::load(int size, qreal devicePixelRatio)
{
    if (!sprites.isEmpty()) {
        return;
    }

    if (const auto raster = std::get_if<CursorThemeXEntryInfo>(&info)) {
        sprites = XCursorReader::load(raster->path, size, devicePixelRatio);
    } else if (const auto svg = std::get_if<CursorThemeSvgEntryInfo>(&info)) {
        sprites = SvgCursorReader::load(svg->path, size, devicePixelRatio);
    }
}

void CursorThemePrivate::discoverXCursors(const QString &packagePath)
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
        registry.insert(shape, std::make_shared<CursorThemeEntry>(CursorThemeXEntryInfo{
            .path = entry.absoluteFilePath(),
        }));
    }
}

void CursorThemePrivate::discoverSvgCursors(const QString &packagePath)
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
        registry.insert(shape, std::make_shared<CursorThemeEntry>(CursorThemeSvgEntryInfo{
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

void CursorThemePrivate::discover(const QStringList &searchPaths)
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

CursorTheme::CursorTheme()
    : d(new CursorThemePrivate)
{
}

CursorTheme::CursorTheme(const QString &themeName, int size, qreal devicePixelRatio, const QStringList &searchPaths)
    : d(new CursorThemePrivate(themeName, size, devicePixelRatio))
{
    d->discover(searchPaths);
}

CursorTheme::CursorTheme(const CursorTheme &other)
    : d(other.d)
{
}

CursorTheme::~CursorTheme()
{
}

CursorTheme &CursorTheme::operator=(const CursorTheme &other)
{
    d = other.d;
    return *this;
}

bool CursorTheme::operator==(const CursorTheme &other)
{
    return d == other.d;
}

bool CursorTheme::operator!=(const CursorTheme &other)
{
    return !(*this == other);
}

QString CursorTheme::name() const
{
    return d->name;
}

int CursorTheme::size() const
{
    return d->size;
}

qreal CursorTheme::devicePixelRatio() const
{
    return d->devicePixelRatio;
}

bool CursorTheme::isEmpty() const
{
    return d->registry.isEmpty();
}

QList<CursorSprite> CursorTheme::shape(const QByteArray &name) const
{
    if (auto entry = d->registry.value(name)) {
        entry->load(d->size, d->devicePixelRatio);
        return entry->sprites;
    }
    return QList<CursorSprite>();
}

} // namespace KWin
