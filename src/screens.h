/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCREENS_H
#define KWIN_SCREENS_H

// KWin includes
#include <kwinglobals.h>
// Qt includes
#include <QObject>

namespace KWin
{

class KWIN_EXPORT Screens : public QObject
{
    Q_OBJECT

public:
    explicit Screens();

    void init();

Q_SIGNALS:
    /**
     * Emitted whenever the screens are changed either count or geometry.
     */
    void changed();
};
}

#endif // KWIN_SCREENS_H
