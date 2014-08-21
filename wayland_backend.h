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

namespace KWin
{

namespace Wayland
{
class ShmPool;
class WaylandBackend;
class WaylandSeat;
class Compositor;
class ConnectionThread;
class FullscreenShell;
class Keyboard;
class Output;
class Pointer;
class Registry;
class Seat;
class Shell;
class ShellSurface;
class Surface;

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
    explicit X11CursorTracker(WaylandSeat *seat, WaylandBackend *backend, QObject* parent = 0);
    virtual ~X11CursorTracker();
    void resetCursor();
private Q_SLOTS:
    void cursorChanged(uint32_t serial);
private:
    void installCursor(const CursorData &cursor);
    WaylandSeat *m_seat;
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
    Seat *m_seat;
    Pointer *m_pointer;
    Keyboard *m_keyboard;
    Surface *m_cursor;
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
    Compositor *compositor();
    const QList<Output*> &outputs() const;
    ShmPool *shmPool();

    Surface *surface() const;
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
    wl_event_queue *m_eventQueue;
    Registry *m_registry;
    Compositor *m_compositor;
    Shell *m_shell;
    Surface *m_surface;
    ShellSurface *m_shellSurface;
    QScopedPointer<WaylandSeat> m_seat;
    ShmPool *m_shm;
    QList<Output*> m_outputs;
    ConnectionThread *m_connectionThreadObject;
    QThread *m_connectionThread;
    FullscreenShell *m_fullscreenShell;

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
Compositor *WaylandBackend::compositor()
{
    return m_compositor;
}

inline
ShmPool* WaylandBackend::shmPool()
{
    return m_shm;
}

inline
Surface *WaylandBackend::surface() const
{
    return m_surface;
}

inline
const QList< Output* >& WaylandBackend::outputs() const
{
    return m_outputs;
}

} // namespace Wayland
} // namespace KWin

#endif //  KWIN_WAYLAND_BACKEND_H
