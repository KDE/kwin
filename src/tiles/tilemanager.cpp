/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tilemanager.h"
#include "core/backendoutput.h"
#include "quicktile.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

namespace KWin
{

QDebug operator<<(QDebug debug, const TileManager *tileManager)
{
    if (tileManager) {
        QList<Tile *> tiles({tileManager->rootTile(VirtualDesktopManager::self()->currentDesktop())});
        QList<Tile *> tilePath;
        QString indent(QStringLiteral("|-"));
        debug << tileManager->metaObject()->className() << '(' << static_cast<const void *>(tileManager) << ')' << '\n';
        while (!tiles.isEmpty()) {
            auto *tile = tiles.last();
            tiles.pop_back();
            debug << indent << qobject_cast<CustomTile *>(tile) << '\n';
            if (tile->childCount() > 0) {
                tiles.append(tile->childTiles());
                tilePath.append(tile->childTiles().first());
                indent.prepend(QStringLiteral("| "));
            }
            if (!tilePath.isEmpty() && tile == tilePath.last()) {
                tilePath.pop_back();
                indent.remove(0, 2);
            }
        }

    } else {
        debug << "Tile(0x0)";
    }
    return debug;
}

TileManager::TileManager(LogicalOutput *parent)
    : QObject(parent)
    , m_output(parent)
{
    m_saveTimer = std::make_unique<QTimer>(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(2000);
    connect(m_saveTimer.get(), &QTimer::timeout, this, &TileManager::saveSettings);

    auto addDesktop = [this](VirtualDesktop *desk) {
        RootTile *rootTile = new RootTile(this, desk);
        m_rootTiles[desk] = rootTile;
        m_quickRootTiles[desk] = new QuickRootTile(this, desk);

        rootTile->setRelativeGeometry(RectF(0, 0, 1, 1));
        connect(rootTile, &CustomTile::paddingChanged, m_saveTimer.get(), static_cast<void (QTimer::*)()>(&QTimer::start));
        connect(rootTile, &CustomTile::layoutModified, m_saveTimer.get(), static_cast<void (QTimer::*)()>(&QTimer::start));

        readSettings(rootTile);
    };

    for (VirtualDesktop *desk : VirtualDesktopManager::self()->desktops()) {
        addDesktop(desk);
    }

    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::desktopAdded, this, addDesktop);
    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::desktopRemoved,
            this, [this](VirtualDesktop *desk) {
        delete m_rootTiles.take(desk);
        delete m_quickRootTiles.take(desk);
    });
    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::currentChanged,
            this, [this](VirtualDesktop *oldDesk, VirtualDesktop *newDesk) {
        Q_EMIT rootTileChanged(rootTile());
        Q_EMIT modelChanged(model());
    });
}

TileManager::~TileManager()
{
    m_tearingDown = true;
}

bool TileManager::tearingDown() const
{
    return m_tearingDown;
}

LogicalOutput *TileManager::output() const
{
    return m_output;
}

Tile *TileManager::bestTileForPosition(qreal x, qreal y)
{
    return rootTile()->pick(QPointF(x, y));
}

RootTile *TileManager::rootTile(VirtualDesktop *desktop) const
{
    return m_rootTiles.value(desktop);
}

RootTile *TileManager::rootTile() const
{
    return m_rootTiles.value(VirtualDesktopManager::self()->currentDesktop());
}

QuickRootTile *TileManager::quickRootTile() const
{
    return m_quickRootTiles.value(VirtualDesktopManager::self()->currentDesktop());
}

QuickRootTile *TileManager::quickRootTile(VirtualDesktop *desktop) const
{
    return m_quickRootTiles.value(desktop);
}

Tile *TileManager::quickTile(QuickTileMode mode) const
{
    return quickRootTile()->tileForMode(mode);
}

TileModel *TileManager::model() const
{
    return rootTile()->model();
}

Tile *TileManager::tileForWindow(Window *window, VirtualDesktop *desktop)
{
    if (!window || !desktop) {
        return nullptr;
    }
    Q_ASSERT(m_rootTiles.contains(desktop));
    Q_ASSERT(m_quickRootTiles.contains(desktop));

    Tile *owner = m_quickRootTiles[desktop]->tileForWindow(window);
    if (owner) {
        return owner;
    }
    return m_rootTiles[desktop]->tileForWindow(window);
}

void TileManager::forgetWindow(Window *window, VirtualDesktop *desktop)
{
    if (!window) {
        return;
    }

    if (desktop) {
        Tile *owner = tileForWindow(window, desktop);
        if (owner) {
            owner->forget(window);
        }
        return;
    }

    const QList<VirtualDesktop *> desktops = VirtualDesktopManager::self()->desktops();
    for (VirtualDesktop *desk : desktops) {
        Tile *owner = tileForWindow(window, desk);
        if (owner) {
            owner->forget(window);
        }
    }
}

Tile::LayoutDirection strToLayoutDirection(const QString &dir)
{
    if (dir == QLatin1StringView("horizontal")) {
        return Tile::LayoutDirection::Horizontal;
    } else if (dir == QLatin1StringView("vertical")) {
        return Tile::LayoutDirection::Vertical;
    } else {
        return Tile::LayoutDirection::Floating;
    }
}

CustomTile *TileManager::parseTilingJSon(const QJsonValue &val, const RectF &availableArea, CustomTile *parentTile)
{
    if (availableArea.isEmpty()) {
        return nullptr;
    }

    if (val.isObject()) {
        const auto &obj = val.toObject();
        CustomTile *createdTile = nullptr;

        if (parentTile->layoutDirection() == Tile::LayoutDirection::Horizontal) {
            RectF rect = availableArea;
            const auto width = obj.value(QStringLiteral("width"));
            if (width.isDouble()) {
                rect.setWidth(std::min(width.toDouble(), availableArea.width()));
            }
            if (!rect.isEmpty()) {
                createdTile = parentTile->createChildAt(rect, parentTile->layoutDirection(), parentTile->childCount());
            }

        } else if (parentTile->layoutDirection() == Tile::LayoutDirection::Vertical) {
            RectF rect = availableArea;
            const auto height = obj.value(QStringLiteral("height"));
            if (height.isDouble()) {
                rect.setHeight(std::min(height.toDouble(), availableArea.height()));
            }
            if (!rect.isEmpty()) {
                createdTile = parentTile->createChildAt(rect, parentTile->layoutDirection(), parentTile->childCount());
            }

        } else if (parentTile->layoutDirection() == Tile::LayoutDirection::Floating) {
            RectF rect(0, 0, 1, 1);
            rect = RectF(obj.value(QStringLiteral("x")).toDouble(),
                         obj.value(QStringLiteral("y")).toDouble(),
                         obj.value(QStringLiteral("width")).toDouble(),
                         obj.value(QStringLiteral("height")).toDouble());

            if (!rect.isEmpty()) {
                createdTile = parentTile->createChildAt(rect, parentTile->layoutDirection(), parentTile->childCount());
            }
        }

        if (createdTile && obj.contains(QStringLiteral("tiles"))) {
            // It's a layout
            const auto arr = obj.value(QStringLiteral("tiles"));
            const auto direction = obj.value(QStringLiteral("layoutDirection"));
            // Ignore arrays with only a single item in it
            if (arr.isArray() && arr.toArray().count() > 0) {
                const Tile::LayoutDirection dir = strToLayoutDirection(direction.toString());
                createdTile->setLayoutDirection(dir);
                parseTilingJSon(arr, createdTile->relativeGeometry(), createdTile);
            }
        }
        return createdTile;
    } else if (val.isArray()) {
        const auto arr = val.toArray();
        auto avail = availableArea;
        for (auto it = arr.cbegin(); it != arr.cend(); it++) {
            if ((*it).isObject()) {
                auto *tile = parseTilingJSon(*it, avail, parentTile);
                if (tile && parentTile->layoutDirection() == Tile::LayoutDirection::Horizontal) {
                    avail.setLeft(tile->relativeGeometry().right());
                } else if (tile && parentTile->layoutDirection() == Tile::LayoutDirection::Vertical) {
                    avail.setTop(tile->relativeGeometry().bottom());
                }
            }
        }
        // make sure the children fill exactly the parent, eventually enlarging the last
        if (parentTile->layoutDirection() != Tile::LayoutDirection::Floating
            && parentTile->childCount() > 0) {
            auto *last = parentTile->childTile(parentTile->childCount() - 1);
            auto geom = last->relativeGeometry();
            geom.setRight(parentTile->relativeGeometry().right());
            last->setRelativeGeometry(geom);
        }
        return nullptr;
    }
    return nullptr;
}

// this is the old output UUID format from Plasma 6.3
// to not reset the config on updates, attempt to load the settings with this uuid too!
static QString generateOutputId(LogicalOutput *output)
{
    static const QUuid urlNs = QUuid("6ba7b811-9dad-11d1-80b4-00c04fd430c8"); // NameSpace_URL
    static const QUuid kwinNs = QUuid::createUuidV5(urlNs, QStringLiteral("https://kwin.kde.org/o/"));

    const QString payload = QStringList{output->name(), output->backendOutput()->eisaId(), output->model(), output->serialNumber()}.join(':');
    return QUuid::createUuidV5(kwinNs, payload).toString(QUuid::StringFormat::WithoutBraces);
}

void TileManager::readSettings(RootTile *rootTile)
{
    KConfigGroup cg = kwinApp()->config()->group(QStringLiteral("Tiling"));
    qreal padding = cg.readEntry("padding", 4);
    VirtualDesktop *desk = rootTile->desktop();
    KConfigGroup desktopCg = KConfigGroup(&cg, desk->id());
    cg = KConfigGroup(&desktopCg, m_output->uuid());

    Q_ASSERT(m_rootTiles.contains(desk));

    auto createDefaultSetup = [](RootTile *rootTile) {
        Q_ASSERT(rootTile->childCount() == 0);
        // If empty create an horizontal 3 columns layout
        rootTile->setLayoutDirection(Tile::LayoutDirection::Horizontal);
        rootTile->split(Tile::LayoutDirection::Horizontal);
        static_cast<CustomTile *>(rootTile->childTile(0))->split(Tile::LayoutDirection::Horizontal);
        Q_ASSERT(rootTile->childCount() == 3);
        // Resize middle column, the other two will be auto resized accordingly
        rootTile->childTile(1)->setRelativeGeometry({0.25, 0.0, 0.5, 1.0});
    };

    QJsonParseError error;
    auto tiles = cg.readEntry("tiles", QByteArray());
    if (tiles.isEmpty()) {
        cg = KConfigGroup(&desktopCg, generateOutputId(m_output));
        tiles = cg.readEntry("tiles", QByteArray());
        if (tiles.isEmpty()) {
            qCDebug(KWIN_CORE) << "Empty tiles configuration for monitor" << m_output->uuid() << ":"
                               << "Creating default setup";
            createDefaultSetup(rootTile);
            return;
        }
    }
    QJsonDocument doc = QJsonDocument::fromJson(tiles, &error);

    if (error.error != QJsonParseError::NoError) {
        qCWarning(KWIN_CORE) << "Parse error in tiles configuration for monitor" << m_output->uuid() << ":" << error.errorString() << "Creating default setup";
        createDefaultSetup(rootTile);
        return;
    }

    if (doc.object().contains(QStringLiteral("tiles"))) {
        const auto arr = doc.object().value(QStringLiteral("tiles"));
        if (arr.isArray() && arr.toArray().count() > 0) {
            rootTile->setLayoutDirection(strToLayoutDirection(doc.object().value(QStringLiteral("layoutDirection")).toString()));
            parseTilingJSon(arr, RectF(0, 0, 1, 1), rootTile);
        }
    }

    rootTile->setPadding(padding);
}

QJsonObject TileManager::tileToJSon(CustomTile *tile)
{
    QJsonObject obj;

    auto *parentTile = static_cast<CustomTile *>(tile->parentTile());

    // Exclude the root and the two children
    if (parentTile) {
        switch (parentTile->layoutDirection()) {
        case Tile::LayoutDirection::Horizontal:
            obj[QStringLiteral("width")] = tile->relativeGeometry().width();
            break;
        case Tile::LayoutDirection::Vertical:
            obj[QStringLiteral("height")] = tile->relativeGeometry().height();
            break;
        case Tile::LayoutDirection::Floating:
        default:
            obj[QStringLiteral("x")] = tile->relativeGeometry().x();
            obj[QStringLiteral("y")] = tile->relativeGeometry().y();
            obj[QStringLiteral("width")] = tile->relativeGeometry().width();
            obj[QStringLiteral("height")] = tile->relativeGeometry().height();
        }
    }

    if (tile->isLayout()) {
        switch (tile->layoutDirection()) {
        case Tile::LayoutDirection::Horizontal:
            obj[QStringLiteral("layoutDirection")] = QStringLiteral("horizontal");
            break;
        case Tile::LayoutDirection::Vertical:
            obj[QStringLiteral("layoutDirection")] = QStringLiteral("vertical");
            break;
        case Tile::LayoutDirection::Floating:
        default:
            obj[QStringLiteral("layoutDirection")] = QStringLiteral("floating");
        }

        QJsonArray tiles;
        const int nChildren = tile->childCount();
        for (int i = 0; i < nChildren; ++i) {
            tiles.append(tileToJSon(static_cast<CustomTile *>(tile->childTile(i))));
        }
        obj[QStringLiteral("tiles")] = tiles;
    }

    return obj;
}

void TileManager::saveSettings()
{
    KConfigGroup cg = kwinApp()->config()->group(QStringLiteral("Tiling"));
    cg.writeEntry("padding", rootTile()->padding());

    for (auto it = m_rootTiles.constBegin(); it != m_rootTiles.constEnd(); it++) {
        VirtualDesktop *desk = it.key();
        RootTile *rootTile = it.value();
        auto obj = tileToJSon(rootTile);
        QJsonDocument doc(obj);
        KConfigGroup tileGroup(&cg, desk->id());
        tileGroup = KConfigGroup(&tileGroup, m_output->uuid());
        tileGroup.writeEntry("tiles", doc.toJson(QJsonDocument::Compact));
    }
    cg.sync(); // FIXME: less frequent?
}

} // namespace KWin

#include "moc_tilemanager.cpp"
