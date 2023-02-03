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
    : QObject(parent)
    , m_output(parent)
    , m_tileModel(new TileModel(this))
{
    m_saveTimer.reset(new QTimer(this));
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(2000);
    connect(m_saveTimer.get(), &QTimer::timeout, this, &TileManager::saveSettings);

    m_rootTile = std::make_unique<RootTile>(this);
    m_rootTile->setRelativeGeometry(QRectF(0, 0, 1, 1));
    connect(m_rootTile.get(), &CustomTile::paddingChanged, m_saveTimer.get(), static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_rootTile.get(), &CustomTile::layoutModified, m_saveTimer.get(), static_cast<void (QTimer::*)()>(&QTimer::start));

    m_quickRootTile = std::make_unique<QuickRootTile>(this);

    readSettings();
}

TileManager::~TileManager()
{
}

Output *TileManager::output() const
{
    return m_output;
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
            qreal distance = (r.center() - pos).manhattanLength();
            if (!r.contains(pos)) {
                // This gives a strong preference for tiles that contain the point
                // still base on distance though as floating tiles can overlap
                distance += m_output->fractionalGeometry().width();
            }
            if (distance < minimumDistance) {
                minimumDistance = distance;
                ret = t;
            }
        }
    }
    return ret;
}

Tile *TileManager::bestTileForPosition(qreal x, qreal y)
{
    return bestTileForPosition({x, y});
}

CustomTile *TileManager::rootTile() const
{
    return m_rootTile.get();
}

Tile *TileManager::quickTile(QuickTileMode mode) const
{
    return m_quickRootTile->tileForMode(mode);
}

TileModel *TileManager::model() const
{
    return m_tileModel.get();
}

Tile::LayoutDirection strToLayoutDirection(const QString &dir)
{
    if (dir == QStringLiteral("horizontal")) {
        return Tile::LayoutDirection::Horizontal;
    } else if (dir == QStringLiteral("vertical")) {
        return Tile::LayoutDirection::Vertical;
    } else {
        return Tile::LayoutDirection::Floating;
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

        if (parentTile->layoutDirection() == Tile::LayoutDirection::Horizontal) {
            QRectF rect = availableArea;
            const auto width = obj.value(QStringLiteral("width"));
            if (width.isDouble()) {
                rect.setWidth(std::min(width.toDouble(), availableArea.width()));
            }
            if (!rect.isEmpty()) {
                createdTile = parentTile->createChildAt(rect, parentTile->layoutDirection(), parentTile->childCount());
            }

        } else if (parentTile->layoutDirection() == Tile::LayoutDirection::Vertical) {
            QRectF rect = availableArea;
            const auto height = obj.value(QStringLiteral("height"));
            if (height.isDouble()) {
                rect.setHeight(std::min(height.toDouble(), availableArea.height()));
            }
            if (!rect.isEmpty()) {
                createdTile = parentTile->createChildAt(rect, parentTile->layoutDirection(), parentTile->childCount());
            }

        } else if (parentTile->layoutDirection() == Tile::LayoutDirection::Floating) {
            QRectF rect(0, 0, 1, 1);
            rect = QRectF(obj.value(QStringLiteral("x")).toDouble(),
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

void TileManager::readSettings()
{
    KConfigGroup cg = kwinApp()->config()->group(QStringLiteral("Tiling"));
    qreal padding = cg.readEntry("padding", 4);
    cg = KConfigGroup(&cg, m_output->uuid().toString(QUuid::WithoutBraces));

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(cg.readEntry("tiles", QByteArray()), &error);

    auto createDefaultSetup = [this]() {
        Q_ASSERT(m_rootTile->childCount() == 0);
        // If empty create an horizontal 3 columns layout
        m_rootTile->setLayoutDirection(Tile::LayoutDirection::Horizontal);
        m_rootTile->split(Tile::LayoutDirection::Horizontal);
        static_cast<CustomTile *>(m_rootTile->childTile(0))->split(Tile::LayoutDirection::Horizontal);
        Q_ASSERT(m_rootTile->childCount() == 3);
        // Resize middle column, the other two will be auto resized accordingly
        m_rootTile->childTile(1)->setRelativeGeometry({0.25, 0.0, 0.5, 1.0});
    };

    if (error.error != QJsonParseError::NoError) {
        qCWarning(KWIN_CORE) << "Parse error in tiles configuration for monitor" << m_output->uuid().toString(QUuid::WithoutBraces) << ":" << error.errorString() << "Creating default setup";
        createDefaultSetup();
        return;
    }

    if (doc.object().contains(QStringLiteral("tiles"))) {
        const auto arr = doc.object().value(QStringLiteral("tiles"));
        if (arr.isArray() && arr.toArray().count() > 0) {
            m_rootTile->setLayoutDirection(strToLayoutDirection(doc.object().value(QStringLiteral("layoutDirection")).toString()));
            parseTilingJSon(arr, QRectF(0, 0, 1, 1), m_rootTile.get());
        }
    }

    m_rootTile->setPadding(padding);
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
    auto obj = tileToJSon(m_rootTile.get());
    QJsonDocument doc(obj);
    KConfigGroup cg = kwinApp()->config()->group(QStringLiteral("Tiling"));
    cg.writeEntry("padding", m_rootTile->padding());
    cg = KConfigGroup(&cg, m_output->uuid().toString(QUuid::WithoutBraces));
    cg.writeEntry("tiles", doc.toJson(QJsonDocument::Compact));
    cg.sync(); // FIXME: less frequent?
}

} // namespace KWin

#include "moc_tilemanager.cpp"
