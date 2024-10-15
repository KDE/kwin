/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

#include "window.h"

#include <kwin_export.h>

namespace KWin
{

/**
 * Custom tiling zones management per output.
 */
class KWIN_EXPORT QuickTileLayout : public QObject
{
    Q_OBJECT

public:
    explicit QuickTileLayout(QObject *parent = nullptr);
    ~QuickTileLayout() override;

    void setAssociation(Window *window, QuickTileMode tile);
    bool removeAssociation(Window *window);

    QuickTileMode modeForWindow(Window *window) const;
    QList<Window *> windowsForMode(QuickTileMode tile) const;

    QHash<Window *, QuickTileMode> windowTileMap() const;

Q_SIGNALS:
    void windowAssociated(Window *window, KWin::QuickTileMode tile);
    void associationRemoved(Window *window, KWin::QuickTileMode tile);

private:
    QHash<Window *, QuickTileMode> m_modeForWindow;
    QMultiHash<QuickTileMode, Window *> m_windowsForMode;
};

KWIN_EXPORT QDebug operator<<(QDebug debug, const QuickTileLayout *tileLayout);

} // namespace KWin
