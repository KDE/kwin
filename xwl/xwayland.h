/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2019 Roman Gilg <subdiff@gmail.com>

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
#ifndef KWIN_XWL_XWAYLAND
#define KWIN_XWL_XWAYLAND

#include <QObject>

class QProcess;

class xcb_screen_t;

namespace KWin
{
class ApplicationWayland;

namespace Xwl
{

class Xwayland : public QObject
{
    Q_OBJECT
public:
    Xwayland(ApplicationWayland *app, QObject *parent = nullptr);
    virtual ~Xwayland();

    void init();

Q_SIGNALS:
    void criticalError(int code);

private:
    void createX11Connection();
    void continueStartupWithX();

    int m_xcbConnectionFd = -1;
    QProcess *m_xwaylandProcess = nullptr;
    QMetaObject::Connection m_xwaylandFailConnection;

    ApplicationWayland *m_app;
};

}
}

#endif
