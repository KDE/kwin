/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_WAYLAND_SERVER_H
#define KWIN_WAYLAND_SERVER_H

#include <kwinglobals.h>

#include <QObject>

namespace KWayland
{
namespace Server
{
class ClientConnection;
class CompositorInterface;
class Display;
class ShellInterface;
class SeatInterface;
class OutputInterface;
}
}

namespace KWin
{

class AbstractBackend;

class KWIN_EXPORT WaylandServer : public QObject
{
    Q_OBJECT
public:
    virtual ~WaylandServer();
    void init(const QByteArray &socketName = QByteArray());
    void initOutputs();

    KWayland::Server::Display *display() {
        return m_display;
    }
    KWayland::Server::CompositorInterface *compositor() {
        return m_compositor;
    }
    KWayland::Server::SeatInterface *seat() {
        return m_seat;
    }
    KWayland::Server::ShellInterface *shell() {
        return m_shell;
    }

    AbstractBackend *backend() const {
        return m_backend;
    }
    void installBackend(AbstractBackend *backend);
    void uninstallBackend(AbstractBackend *backend);

    /**
     * @returns file descriptor for Xwayland to connect to.
     **/
    int createXWaylandConnection();

    /**
     * @returns file descriptor for QtWayland
     **/
    int createQtConnection();

    KWayland::Server::ClientConnection *xWaylandConnection() const {
        return m_xwaylandConnection;
    }

private:
    KWayland::Server::Display *m_display = nullptr;
    KWayland::Server::CompositorInterface *m_compositor = nullptr;
    KWayland::Server::SeatInterface *m_seat = nullptr;
    KWayland::Server::ShellInterface *m_shell = nullptr;
    KWayland::Server::ClientConnection *m_xwaylandConnection = nullptr;
    KWayland::Server::ClientConnection *m_qtConnection = nullptr;
    AbstractBackend *m_backend = nullptr;
    KWIN_SINGLETON(WaylandServer)
};

inline
WaylandServer *waylandServer() {
    return WaylandServer::self();
}

} // namespace KWin

#endif

