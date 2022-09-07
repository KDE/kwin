/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tilemanager.h"
#include "core/output.h"
#include "quicktile.h"
#include "virtualdesktops.h"
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
        QList<Tile *> tiles({tileManager->rootTile()});
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

TileManager::TileManager(Output *parent)
    : QAbstractItemModel(parent)
    , m_output(parent)
{
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(2000);
    connect(m_saveTimer, &QTimer::timeout, this, &TileManager::saveSettings);

    m_rootTile = std::unique_ptr<CustomTile>(new RootTile(this));
    m_rootTile->setRelativeGeometry(QRectF(0, 0, 1, 1));
    connect(m_rootTile.get(), &CustomTile::layoutModified, m_saveTimer, static_cast<void (QTimer::*)()>(&QTimer::start));

    m_quickRootTile = std::unique_ptr<QuickRootTile>(new QuickRootTile(this));

    connect(m_output, &Output::informationChanged, this, &TileManager::readSettings);
}

TileManager::~TileManager()
{
}

Output *TileManager::output() const
{
    return m_output;
}

QHash<int, QByteArray> TileManager::roleNames() const
{
    return {
        {TileRole, QByteArrayLiteral("tile")}};
}

Tile *TileManager::bestTileForPosition(const QPointF &pos)
{
    const auto tiles = m_rootTile->descendants();
    qreal minimumDistance = std::numeric_limits<qreal>::max();
    Tile *ret = nullptr;

    for (auto *t : tiles) {
        if (!t->isLayout()) {
            const auto r = t->absoluteGeometry();
            // It's possible for tiles to overlap, so take the one which center is nearer to mouse pos
            const qreal distance = (r.center() - pos).manhattanLength();
            // TODO: decide if we want contains or not
            if (/*r.contains(pos) && */ distance < minimumDistance) {
                minimumDistance = distance;
                ret = t;
            }
        }
    }
    return ret;
}

Tile *TileManager::quickTileForPosition(const QPointF &pos)
{
    const auto hotSideMargin = 20;
    const auto hotTopMargin = 5;
    const auto maxGeom = m_quickRootTile->maximizedWindowGeometry();
    if (!maxGeom.contains(pos)) {
        return nullptr;
    }

    const auto checkSideTiles = [this](const QPointF &pos, QuickTileFlag baseFlag) {
        const auto flags = {QuickTileFlag::Top, QuickTileFlag::Bottom};
        auto *nearestTile = quickTile(baseFlag);
        auto nearestDistance = (nearestTile->windowGeometry().center() - pos).manhattanLength();
        for (const auto flag : flags) {
            auto *tile = quickTile(baseFlag | flag);
            const auto distance = (tile->windowGeometry().center() - pos).manhattanLength();
            if (distance < nearestDistance) {
                nearestTile = tile;
                nearestDistance = distance;
            }
        }
        return nearestTile;
    };

    if (options->electricBorderTiling() && pos.x() - maxGeom.x() <= hotSideMargin) {
        return checkSideTiles(pos, QuickTileFlag::Left);
    } else if (options->electricBorderTiling() && maxGeom.right() - pos.x() <= hotSideMargin) {
        return checkSideTiles(pos, QuickTileFlag::Right);
    } else if (options->electricBorderMaximize() && pos.y() - maxGeom.top() <= hotTopMargin) {
        return maximizeTile(pos);
    }
    return nullptr;
}

QVariant TileManager::bestTileForPosition(qreal x, qreal y)
{
    return QVariant::fromValue(bestTileForPosition({x, y}));
}

CustomTile *TileManager::rootTile() const
{
    return m_rootTile.get();
}

Tile *TileManager::quickTile(QuickTileMode mode) const
{
    return m_quickRootTile->tileForMode(mode);
}

KWin::Tile *TileManager::maximizeTile(const QPointF &pos)
{
    auto *t = bestTileForPosition(pos);
    if (t && t->isMaximizeArea()) {
        return t;
    } else {
        return m_quickRootTile.get();
    }
}

QVariant TileManager::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role == TileRole) {
        return QVariant::fromValue(static_cast<CustomTile *>(index.internalPointer()));
    }

    return QVariant();
}

Qt::ItemFlags TileManager::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    return QAbstractItemModel::flags(index);
}

QModelIndex TileManager::index(int row, int column, const QModelIndex &parent) const
{
    if (column > 0 || !hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    CustomTile *parentItem;

    if (!parent.isValid()) {
        parentItem = m_rootTile.get();
    } else {
        parentItem = static_cast<CustomTile *>(parent.internalPointer());
    }

    CustomTile *childItem = static_cast<CustomTile *>(parentItem->childTile(row));
    if (childItem) {
        return createIndex(row, column, childItem);
    }
    return QModelIndex();
}

QModelIndex TileManager::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    CustomTile *childItem = static_cast<CustomTile *>(index.internalPointer());
    CustomTile *parentItem = static_cast<CustomTile *>(childItem->parentTile());

    if (!parentItem || parentItem == m_rootTile.get()) {
        return QModelIndex();
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

int TileManager::rowCount(const QModelIndex &parent) const
{
    Tile *parentItem;
    if (parent.column() > 0) {
        return 0;
    }

    if (!parent.isValid()) {
        parentItem = m_rootTile.get();
    } else {
        parentItem = static_cast<CustomTile *>(parent.internalPointer());
    }

    return parentItem->childCount();
}

int TileManager::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

CustomTile::LayoutDirection strToLayoutDirection(const QString &dir)
{
    if (dir == QStringLiteral("horizontal")) {
        return CustomTile::LayoutDirection::Horizontal;
    } else if (dir == QStringLiteral("vertical")) {
        return CustomTile::LayoutDirection::Vertical;
    } else {
        return CustomTile::LayoutDirection::Floating;
    }
}

CustomTile *TileManager::parseTilingJSon(const QJsonValue &val, const QRectF &availableArea, CustomTile *parentTile)
{
    if (availableArea.isEmpty()) {
        return nullptr;
    }

    if (val.isObject()) {
        const auto &obj = val.toObject();
        CustomTile *createdTile = nullptr;

        if (parentTile->layoutDirection() == CustomTile::LayoutDirection::Horizontal) {
            QRectF rect = availableArea;
            const auto width = obj.value(QStringLiteral("width"));
            if (width.isDouble()) {
                rect.setWidth(qMin(width.toDouble(), availableArea.width()));
            }
            if (!rect.isEmpty()) {
                createdTile = addTile(rect, parentTile->layoutDirection(), -1, parentTile);
            }

        } else if (parentTile->layoutDirection() == CustomTile::LayoutDirection::Vertical) {
            QRectF rect = availableArea;
            const auto height = obj.value(QStringLiteral("height"));
            if (height.isDouble()) {
                rect.setHeight(qMin(height.toDouble(), availableArea.height()));
            }
            if (!rect.isEmpty()) {
                createdTile = addTile(rect, parentTile->layoutDirection(), -1, parentTile);
            }

        } else if (parentTile->layoutDirection() == CustomTile::LayoutDirection::Floating) {
            QRectF rect(0, 0, 1, 1);
            if (parentTile != m_rootTile.get()) {
                rect = QRectF(obj.value(QStringLiteral("x")).toDouble(),
                              obj.value(QStringLiteral("y")).toDouble(),
                              obj.value(QStringLiteral("width")).toDouble(),
                              obj.value(QStringLiteral("height")).toDouble());
            }

            if (!rect.isEmpty()) {
                createdTile = addTile(rect, parentTile->layoutDirection(), -1, parentTile);
            }
        }

        if (createdTile && obj.contains(QStringLiteral("tiles"))) {
            // It's a layout
            const auto arr = obj.value(QStringLiteral("tiles"));
            const auto direction = obj.value(QStringLiteral("layoutDirection"));
            // Ignore arrays with only a single item in it
            if (arr.isArray() && arr.toArray().count() > 0) {
                const CustomTile::LayoutDirection dir = strToLayoutDirection(direction.toString());
                createdTile->setLayoutDirection(dir);
                parseTilingJSon(arr, createdTile->relativeGeometry(), createdTile);
            }
        } else if (createdTile && obj.contains(QStringLiteral("isMaximizeArea"))) {
            createdTile->setMaximizeArea(obj.value(QStringLiteral("isMaximizeArea")).toBool());
        }

        return createdTile;
    } else if (val.isArray()) {
        const auto arr = val.toArray();
        auto avail = availableArea;
        for (auto it = arr.cbegin(); it != arr.cend(); it++) {
            if ((*it).isObject()) {
                auto *tile = parseTilingJSon(*it, avail, parentTile);
                if (tile && parentTile->layoutDirection() == CustomTile::LayoutDirection::Horizontal) {
                    avail.setLeft(tile->relativeGeometry().right());
                } else if (tile && parentTile->layoutDirection() == CustomTile::LayoutDirection::Vertical) {
                    avail.setTop(tile->relativeGeometry().bottom());
                }
            }
        }
        // make sure the children fill exactly the parent, eventually enlarging the last
        if (parentTile->layoutDirection() != CustomTile::LayoutDirection::Floating
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

CustomTile *TileManager::addTile(const QRectF &relativeGeometry, CustomTile::LayoutDirection layoutDirection, int position, CustomTile *parentTile)
{
    auto index = parentTile == m_rootTile.get() ? QModelIndex() : createIndex(parentTile->row(), 0, parentTile);
    const int pos = position < 0 ? parentTile->childCount() : qBound(0, position, parentTile->childCount());

    beginInsertRows(index, pos, pos);
    auto *tile = parentTile->createChild(relativeGeometry, layoutDirection, pos);
    endInsertRows();

    return tile;
}

void TileManager::removeTile(CustomTile *tile)
{
    // Can't delete the root tile
    const auto parentTile = static_cast<CustomTile *>(tile->parentTile());
    if (!parentTile) {
        qCWarning(KWIN_CORE) << "Can't remove the root tile";
        return;
    }

    auto parentIndex = parentTile == m_rootTile.get() ? QModelIndex() : createIndex(parentTile->row(), 0, parentTile);
    beginRemoveRows(parentIndex, tile->row(), tile->row());
    parentTile->destroyChild(tile);
    endRemoveRows();

    Q_EMIT tileRemoved(tile);

    // On linear layouts remove the last one and promote the layout as leaf
    if (parentTile->layoutDirection() != CustomTile::LayoutDirection::Floating && parentTile->childCount() == 1) {
        auto *lastTile = static_cast<CustomTile *>(parentTile->childTile(0));
        if (lastTile->childCount() == 0) {
            removeTile(lastTile);
        }
    }
}

void TileManager::readSettings()
{
    KConfigGroup cg = kwinApp()->config()->group(QStringLiteral("Tiling"));
    cg = KConfigGroup(&cg, m_output->uuid().toString(QUuid::WithoutBraces));

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(cg.readEntry("tiles", QByteArray()), &error);

    if (error.error != QJsonParseError::NoError) {
        qCWarning(KWIN_CORE) << "Parse error in tiles configuration for monitor" << m_output->uuid().toString(QUuid::WithoutBraces) << ":" << error.errorString();
        return;
    }

    if (doc.object().contains(QStringLiteral("tiles"))) {
        const auto arr = doc.object().value(QStringLiteral("tiles"));
        if (arr.isArray() && arr.toArray().count() > 0) {
            m_rootTile->setLayoutDirection(strToLayoutDirection(doc.object().value(QStringLiteral("layoutDirection")).toString()));
            parseTilingJSon(arr, QRectF(0, 0, 1, 1), m_rootTile.get());
        }
    }
    if (m_rootTile->childCount() == 0) {
        // Default to horizontal if empty
        m_rootTile->setLayoutDirection(CustomTile::LayoutDirection::Horizontal);
    }

    qWarning() << this;
}

QJsonObject TileManager::tileToJSon(CustomTile *tile)
{
    QJsonObject obj;

    auto *parentTile = static_cast<CustomTile *>(tile->parentTile());

    // Exclude the root and the two children
    if (parentTile) {
        switch (parentTile->layoutDirection()) {
        case CustomTile::LayoutDirection::Horizontal:
            obj[QStringLiteral("width")] = tile->relativeGeometry().width();
            break;
        case CustomTile::LayoutDirection::Vertical:
            obj[QStringLiteral("height")] = tile->relativeGeometry().height();
            break;
        case CustomTile::LayoutDirection::Floating:
        default:
            obj[QStringLiteral("x")] = tile->relativeGeometry().x();
            obj[QStringLiteral("y")] = tile->relativeGeometry().y();
            obj[QStringLiteral("width")] = tile->relativeGeometry().width();
            obj[QStringLiteral("height")] = tile->relativeGeometry().height();
        }
    }

    if (tile->isMaximizeArea()) {
        obj[QStringLiteral("isMaximizeArea")] = true;
    }

    if (tile->isLayout()) {
        switch (tile->layoutDirection()) {
        case CustomTile::LayoutDirection::Horizontal:
            obj[QStringLiteral("layoutDirection")] = QStringLiteral("horizontal");
            break;
        case CustomTile::LayoutDirection::Vertical:
            obj[QStringLiteral("layoutDirection")] = QStringLiteral("vertical");
            break;
        case CustomTile::LayoutDirection::Floating:
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
    auto obj = tileToJSon(m_rootTile.get());
    QJsonDocument doc(obj);
    KConfigGroup cg = kwinApp()->config()->group(QStringLiteral("Tiling"));
    cg = KConfigGroup(&cg, m_output->uuid().toString(QUuid::WithoutBraces));
    cg.writeEntry("tiles", doc.toJson(QJsonDocument::Compact));
    cg.sync(); // FIXME: less frequent?
}

} // namespace KWin

#include "moc_tilemanager.cpp"
