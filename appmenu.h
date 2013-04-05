/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (c) 2011 Lionel Chauvin <megabigbug@yahoo.fr>
Copyright (c) 2011,2012 Cédric Bellegarde <gnumdk@gmail.com>
Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_APPLICATIONMENU_H
#define KWIN_APPLICATIONMENU_H
// KWin
#include <kwinglobals.h>
// Qt
#include <QObject>
// xcb
#include <xcb/xcb.h>

class QPoint;

namespace KWin
{

class ApplicationMenu : public QObject
{
    Q_OBJECT

public:
    virtual ~ApplicationMenu();

    bool hasMenu(xcb_window_t window);
    void showApplicationMenu(const QPoint &pos, const xcb_window_t window);

private Q_SLOTS:
    void slotShowRequest(qulonglong wid);
    void slotMenuAvailable(qulonglong wid);
    void slotMenuHidden(qulonglong wid);
    void slotClearMenus();

private:
    QList<xcb_window_t> m_windowsMenu;

    KWIN_SINGLETON(ApplicationMenu)
};

}

#endif // KWIN_APPLICATIONMENU_H
