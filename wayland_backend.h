/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

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
#ifndef KWIN_WAYLAND_BACKEND_H
#define KWIN_WAYLAND_BACKEND_H
// KWin
#include <kwinglobals.h>
// Qt
#include <QHash>
#include <QImage>
#include <QObject>
#include <QPoint>
#include <QSize>

class QTemporaryFile;
struct wl_cursor_theme;
struct wl_buffer;
struct wl_display;
struct wl_event_queue;
struct wl_seat;

namespace KWayland
{
namespace Client
{
class Buffer;
class ShmPool;
class Compositor;
class ConnectionThread;
class EventQueue;
class FullscreenShell;
class Keyboard;
class Output;
class Pointer;
class Registry;
class Seat;
class Shell;
class ShellSurface;
class Surface;
}
}

namespace KWin
{

namespace Wayland
{

class WaylandBackend;
class WaylandSeat;

class CursorData
{
public:
    CursorData();
    ~CursorData();
    bool isValid() const;
    const QPoint &hotSpot() const;
    const QImage &cursor() const;
private:
    bool init();
    QImage m_cursor;
    QPoint m_hotSpot;
    bool m_valid;
};

class X11CursorTracker : public QObject
{
    Q_OBJECT
public:
    explicit X11CursorTracker(WaylandBackend *backend, QObject* parent = 0);
    virtual ~X11CursorTracker();
    void resetCursor();

Q_SIGNALS:
    void cursorImageChanged(QWeakPointer<KWayland::Client::Buffer> image, const QSize &size, const QPoint &hotSpot);

private Q_SLOTS:
    void cursorChanged(uint32_t serial);
private:
    void installCursor(const CursorData &cursor);
    QHash<uint32_t, CursorData> m_cursors;
    WaylandBackend *m_backend;
    uint32_t m_installedCursor;
    uint32_t m_lastX11Cursor;
};

class WaylandSeat : public QObject
{
    Q_OBJECT
public:
    WaylandSeat(wl_seat *seat, WaylandBackend *backend);
    virtual ~WaylandSeat();

    void resetCursor();
    void installCursorImage(wl_buffer *image, const QSize &size, const QPoint &hotspot);
    void installCursorImage(Qt::CursorShape shape);
private Q_SLOTS:
    void loadTheme();
private:
    void destroyPointer();
    void destroyKeyboard();
    void destroyTheme();
    KWayland::Client::Seat *m_seat;
    KWayland::Client::Pointer *m_pointer;
    KWayland::Client::Keyboard *m_keyboard;
    KWayland::Client::Surface *m_cursor;
    wl_cursor_theme *m_theme;
    uint32_t m_enteredSerial;
    QScopedPointer<X11CursorTracker> m_cursorTracker;
    WaylandBackend *m_backend;
};

/**
* @brief Class encapsulating all Wayland data structures needed by the Egl backend.
*
* It creates the connection to the Wayland Compositor, sets up the registry and creates
* the Wayland surface and its shell mapping.
*/
class KWIN_EXPORT WaylandBackend : public QObject
{
    Q_OBJECT
public:
    virtual ~WaylandBackend();
    wl_display *display();
    KWayland::Client::Compositor *compositor();
    const QList<KWayland::Client::Output*> &outputs() const;
    KWayland::Client::ShmPool *shmPool();

    KWayland::Client::Surface *surface() const;
    QSize shellSurfaceSize() const;
    void installCursorImage(Qt::CursorShape shape);
Q_SIGNALS:
    void shellSurfaceSizeChanged(const QSize &size);
    void systemCompositorDied();
    void backendReady();
    void outputsChanged();
    void connectionFailed();
private:
    void initConnection();
    void createSurface();
    void destroyOutputs();
    void checkBackendReady();
    wl_display *m_display;
    KWayland::Client::EventQueue *m_eventQueue;
    KWayland::Client::Registry *m_registry;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::Shell *m_shell;
    KWayland::Client::Surface *m_surface;
    KWayland::Client::ShellSurface *m_shellSurface;
    QScopedPointer<WaylandSeat> m_seat;
    KWayland::Client::ShmPool *m_shm;
    QList<KWayland::Client::Output*> m_outputs;
    KWayland::Client::ConnectionThread *m_connectionThreadObject;
    QThread *m_connectionThread;
    KWayland::Client::FullscreenShell *m_fullscreenShell;

    KWIN_SINGLETON(WaylandBackend)
};

inline
bool CursorData::isValid() const
{
    return m_valid;
}

inline
const QPoint& CursorData::hotSpot() const
{
    return m_hotSpot;
}

inline
const QImage &CursorData::cursor() const
{
    return m_cursor;
}

inline
wl_display *WaylandBackend::display()
{
    return m_display;
}

inline
KWayland::Client::Compositor *WaylandBackend::compositor()
{
    return m_compositor;
}

inline
KWayland::Client::ShmPool* WaylandBackend::shmPool()
{
    return m_shm;
}

inline
KWayland::Client::Surface *WaylandBackend::surface() const
{
    return m_surface;
}

inline
const QList< KWayland::Client::Output* >& WaylandBackend::outputs() const
{
    return m_outputs;
}

} // namespace Wayland
} // namespace KWin

#endif //  KWIN_WAYLAND_BACKEND_H
