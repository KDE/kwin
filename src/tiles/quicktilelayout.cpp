/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "quicktilelayout.h"
#include "workspace.h"

namespace KWin
{

QDebug operator<<(QDebug debug, const QuickTileLayout *tileLayout)
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

QuickTileLayout::QuickTileLayout(QObject *parent)
    : QObject(parent)
{
}

QuickTileLayout::~QuickTileLayout()
{
}

void QuickTileLayout::setAssociation(Window *window, QuickTileMode mode)
{
    if (!window) {
        return;
    }

    auto it = m_modeForWindow.find(window);
    if (mode) {
        if (it != m_modeForWindow.end()) {
            removeAssociation(window);
        }
    } else {
        QuickTileMode oldTile = it.value();
        m_modeForWindow.remove(window);
        m_windowsForMode.remove(oldTile, window);
    }

    m_modeForWindow[window] = mode;
    m_windowsForMode.insert(mode, window);

    connect(Workspace::self(), &Workspace::windowRemoved, this, &QuickTileLayout::removeAssociation);

    Q_EMIT windowAssociated(window, mode);
}

bool QuickTileLayout::removeAssociation(Window *window)
{
    auto it = m_modeForWindow.find(window);
    if (it == m_modeForWindow.end()) {
        return false;
    }

    QuickTileMode mode = it.value();
    m_windowsForMode.remove(mode, window);
    m_modeForWindow.erase(it);
    Q_EMIT associationRemoved(window, mode);
    return true;
}

QuickTileMode QuickTileLayout::modeForWindow(Window *window) const
{
    return m_modeForWindow.value(window);
}

QList<Window *> QuickTileLayout::windowsForMode(QuickTileMode mode) const
{
    return m_windowsForMode.values(mode);
}

QHash<Window *, QuickTileMode> QuickTileLayout::windowTileMap() const
{
    return m_modeForWindow;
}

} // namespace KWin

#include "moc_quicktilelayout.cpp"
