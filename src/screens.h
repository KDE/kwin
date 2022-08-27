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
// KDE includes
#include <KConfig>
#include <KSharedConfig>
// Qt includes
#include <QObject>
#include <QRect>
#include <QTimer>
#include <QVector>

namespace KWin
{
class Window;
class Output;
class Platform;

class KWIN_EXPORT Screens : public QObject
{
    Q_OBJECT

public:
    explicit Screens();

Q_SIGNALS:
    /**
     * Emitted whenever the screens are changed either count or geometry.
     */
    void changed();
};
}

#endif // KWIN_SCREENS_H
