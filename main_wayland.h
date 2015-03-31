/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_MAIN_WAYLAND_H
#define KWIN_MAIN_WAYLAND_H
#include "main.h"

namespace KWin
{

class ApplicationWayland : public Application
{
    Q_OBJECT
public:
    ApplicationWayland(int &argc, char **argv);
    virtual ~ApplicationWayland();

    void setStartXwayland(bool start) {
        m_startXWayland = start;
    }
    void setBackendSize(const QSize &size) {
        m_backendSize = size;
    }
    void setWindowed(bool set) {
        m_windowed = set;
    }
    void setX11Display(const QByteArray &display) {
        m_x11Display = display;
    }
    void setWaylandDisplay(const QByteArray &display) {
        m_waylandDisplay = display;
    }
    void setFramebuffer(const QString &fbdev) {
        m_framebuffer = fbdev;
    }

protected:
    void performStartup() override;

private:
    void createBackend();
    void createX11Connection();
    void continueStartupWithScreens();
    void continueStartupWithX();

    bool m_startXWayland = false;
    int m_xcbConnectionFd = -1;
    QSize m_backendSize;
    bool m_windowed = false;
    QByteArray m_x11Display;
    QByteArray m_waylandDisplay;
    QString m_framebuffer;
};

}

#endif
