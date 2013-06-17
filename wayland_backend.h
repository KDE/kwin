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
#include <QObject>
#include <QPoint>
#include <QSize>
// wayland
#include <wayland-client.h>

class QTemporaryFile;
class QImage;
struct wl_buffer;
struct wl_shm;

namespace KWin
{

namespace Wayland
{
class ShmPool;
class WaylandBackend;

class CursorData
{
public:
    CursorData(ShmPool *pool);
    ~CursorData();
    bool isValid() const;
    const QPoint &hotSpot() const;
    const QSize &size() const;
    wl_buffer *cursor() const;
private:
    bool init(ShmPool *pool);
    wl_buffer *m_cursor;
    QPoint m_hotSpot;
    QSize m_size;
    bool m_valid;
};

class X11CursorTracker : public QObject
{
    Q_OBJECT
public:
    explicit X11CursorTracker(wl_pointer *pointer, WaylandBackend *backend, QObject* parent = 0);
    virtual ~X11CursorTracker();
    void setEnteredSerial(uint32_t serial);
    void resetCursor();
private Q_SLOTS:
    void cursorChanged(uint32_t serial);
private:
    void installCursor(const CursorData &cursor);
    wl_pointer *m_pointer;
    QHash<uint32_t, CursorData> m_cursors;
    WaylandBackend *m_backend;
    wl_surface *m_cursor;
    uint32_t m_enteredSerial;
    uint32_t m_installedCursor;
    uint32_t m_lastX11Cursor;
};

class ShmPool
{
public:
    ShmPool(wl_shm *shm);
    ~ShmPool();
    bool isValid() const;
    wl_buffer *createBuffer(const QImage &image);
private:
    bool createPool();
    wl_shm *m_shm;
    wl_shm_pool *m_pool;
    void *m_poolData;
    size_t m_size;
    QScopedPointer<QTemporaryFile> m_tmpFile;
    bool m_valid;
    int m_offset;
};

class WaylandSeat
{
public:
    WaylandSeat(wl_seat *seat, WaylandBackend *backend);
    virtual ~WaylandSeat();

    void changed(uint32_t capabilities);
    wl_seat *seat();
    void pointerEntered(uint32_t serial);
    void resetCursor();
private:
    void destroyPointer();
    void destroyKeyboard();
    wl_seat *m_seat;
    wl_pointer *m_pointer;
    wl_keyboard *m_keyboard;
    QScopedPointer<X11CursorTracker> m_cursorTracker;
    WaylandBackend *m_backend;
};

/**
* @brief Class encapsulating all Wayland data structures needed by the Egl backend.
*
* It creates the connection to the Wayland Compositor, set's up the registry and creates
* the Wayland surface and it's shell mapping.
*/
class WaylandBackend : public QObject
{
    Q_OBJECT
public:
    virtual ~WaylandBackend();
    wl_display *display();
    wl_registry *registry();
    void setCompositor(wl_compositor *c);
    wl_compositor *compositor();
    void setShell(wl_shell *s);
    wl_shell *shell();
    ShmPool *shmPool();
    void createSeat(uint32_t name);
    void createShm(uint32_t name);
    void ping(uint32_t serial);

    wl_surface *surface() const;
    const QSize &shellSurfaceSize() const;
    void setShellSurfaceSize(const QSize &size);

    void dispatchEvents();
Q_SIGNALS:
    void shellSurfaceSizeChanged(const QSize &size);
private Q_SLOTS:
    void readEvents();
private:
    void createSurface();
    wl_display *m_display;
    wl_registry *m_registry;
    wl_compositor *m_compositor;
    wl_shell *m_shell;
    wl_surface *m_surface;
    wl_shell_surface *m_shellSurface;
    QSize m_shellSurfaceSize;
    QScopedPointer<WaylandSeat> m_seat;
    QScopedPointer<ShmPool> m_shm;

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
wl_buffer* CursorData::cursor() const
{
    return m_cursor;
}

inline
const QSize& CursorData::size() const
{
    return m_size;
}

inline
wl_seat *WaylandSeat::seat()
{
    return m_seat;
}

inline
bool ShmPool::isValid() const
{
    return m_valid;
}

inline
wl_display *WaylandBackend::display()
{
    return m_display;
}

inline
wl_registry *WaylandBackend::registry()
{
    return m_registry;
}

inline
void WaylandBackend::setCompositor(wl_compositor *c)
{
    m_compositor = c;
}

inline
wl_compositor *WaylandBackend::compositor()
{
    return m_compositor;
}

inline
wl_shell *WaylandBackend::shell()
{
    return m_shell;
}

inline
ShmPool* WaylandBackend::shmPool()
{
    return m_shm.data();
}

inline
wl_surface *WaylandBackend::surface() const
{
    return m_surface;
}

inline
const QSize &WaylandBackend::shellSurfaceSize() const
{
    return m_shellSurfaceSize;
}

} // namespace Wayland
} // namespace KWin

#endif //  KWIN_WAYLAND_BACKEND_H
