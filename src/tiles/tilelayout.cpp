/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tilelayout.h"

namespace KWin
{

QDebug operator<<(QDebug debug, const TileLayout *tileLayout)
{
    if (tileLayout) {
        debug << "{";
        for (auto it = tileLayout->windowTileMap().constBegin(); it != tileLayout->windowTileMap().constEnd(); ++it) {
            debug << "(" << it.key() << " -> " << it.value();
        }
        debug << "}";
    } else {
        debug << "{}";
    }
    return debug;
}

TileLayout::TileLayout(QObject *parent)
    : QObject(parent)
{
}

TileLayout::~TileLayout()
{
}

void TileLayout::setAssociation(Window *window, Tile *tile)
{
    if (!window) {
        return;
    }

    auto it = m_tileForWindow.find(window);
    if (tile) {
        if (it != m_tileForWindow.end()) {
            removeAssociation(window, it.value());
        }
    } else {
        Tile *oldTile = it.value();
        m_tileForWindow.remove(window);
        m_windowsForTile.remove(oldTile, window);
    }

    m_tileForWindow[window] = tile;
    m_windowsForTile.insert(tile, window);

    Q_EMIT windowAssociated(window, tile);
}

void TileLayout::removeAssociation(Window *window, Tile *tile)
{
    if (!window || !tile) {
        return;
    }

    m_tileForWindow.remove(window);
    m_windowsForTile.remove(tile, window);

    Q_EMIT associationRemoved(window, tile);
}

Tile *TileLayout::tileForWindow(Window *window) const
{
    return m_tileForWindow.value(window);
}

QList<Window *> TileLayout::windowsForTile(Tile *tile) const
{
    return m_windowsForTile.values(tile);
}

void TileLayout::forgetWindow(Window *window)
{
    auto it = m_tileForWindow.find(window);
    if (it == m_tileForWindow.end()) {
        return;
    }

    Tile *tile = it.value();
    m_windowsForTile.remove(tile, window);
    m_tileForWindow.erase(it);
    Q_EMIT associationRemoved(window, tile);
}

void TileLayout::forgetTile(Tile *tile)
{
    const QList<Window *> windows = m_windowsForTile.values(tile);
    if (windows.isEmpty()) {
        return;
    }

    for (Window *w : std::as_const(windows)) {
        removeAssociation(w, tile);
    }
}

QHash<Window *, Tile *> TileLayout::windowTileMap() const
{
    return m_tileForWindow;
}

} // namespace KWin

#include "moc_tilelayout.cpp"
