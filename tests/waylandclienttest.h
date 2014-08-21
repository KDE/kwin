/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef WAYLANDCLIENTTEST_H
#define WAYLANDCLIENTTEST_H

#include <QObject>
#include <QSize>

namespace KWin
{
namespace Wayland
{
class Compositor;
class ConnectionThread;
class Output;
class Registry;
class ShmPool;
class Surface;
}
}

class QThread;
class QTimer;
struct wl_event_queue;

class WaylandClientTest : public QObject
{
    Q_OBJECT
public:
    explicit WaylandClientTest(QObject *parent = nullptr);
    virtual ~WaylandClientTest();

private:
    void init();
    void render(const QSize &size);
    void render();
    void setupRegistry(KWin::Wayland::Registry *registry);
    void toggleTimer();
    QThread *m_connectionThread;
    KWin::Wayland::ConnectionThread *m_connectionThreadObject;
    wl_event_queue *m_eventQueue;
    KWin::Wayland::Compositor *m_compositor;
    KWin::Wayland::Output *m_output;
    KWin::Wayland::Surface *m_surface;
    KWin::Wayland::ShmPool *m_shm;
    QSize m_currentSize;
    QTimer *m_timer;
};

#endif
