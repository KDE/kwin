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
#ifndef KWIN_WAYLAND_CONNECTION_THREAD_H
#define KWIN_WAYLAND_CONNECTION_THREAD_H

#include <QObject>
#include <QDir>

struct wl_display;
class QFileSystemWatcher;
class QSocketNotifier;

namespace KWin
{
namespace Wayland
{

class ConnectionThread : public QObject
{
    Q_OBJECT
public:
    explicit ConnectionThread(QObject *parent = nullptr);
    virtual ~ConnectionThread();

    wl_display *display();
    /**
     * @returns the name of the socket it connects to.
     **/
    const QString &socketName() const;
    /**
     * Sets the @p socketName to connect to.
     * Only applies if called before calling initConnection.
     * The default socket name is derived from environment variable WAYLAND_DISPLAY
     * and if not set is hard coded to "wayland-0".
     **/
    void setSocketName(const QString &socketName);

public Q_SLOTS:
    void initConnection();

Q_SIGNALS:
    /**
     * Emitted once a connection to a Wayland server is established.
     * Normally emitted after invoking initConnection(), but might also be
     * emitted after re-connecting to another server.
     **/
    void connected();
    /**
     * Emitted if connecting to a Wayland server failed.
     **/
    void failed();
    /**
     * Emitted whenever new events are ready to be read.
     **/
    void eventsRead();
    /**
     * Emitted if the Wayland server connection dies.
     * If the socket reappears, it is tried to reconnect.
     **/
    void connectionDied();

private Q_SLOTS:
    void doInitConnection();

private:
    void setupSocketNotifier();
    void setupSocketFileWatcher();
    wl_display *m_display;
    QString m_socketName;
    QDir m_runtimeDir;
    QSocketNotifier *m_socketNotifier;
    QFileSystemWatcher *m_socketWatcher;
    bool m_serverDied;
};

inline
const QString &ConnectionThread::socketName() const
{
    return m_socketName;
}

inline
wl_display *ConnectionThread::display()
{
    return m_display;
}

}
}


#endif
