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
#include <QDir>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QPoint>
#include <QSize>
// wayland
#include <wayland-client.h>

class QTemporaryFile;
class QImage;
class QFileSystemWatcher;
struct wl_cursor_theme;
struct wl_buffer;
struct wl_shm;

namespace KWin
{

namespace Wayland
{
class ShmPool;
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

class Buffer
{
public:
    Buffer(wl_buffer *buffer, const QSize &size, int32_t stride, size_t offset);
    ~Buffer();
    void copy(const void *src);
    void setReleased(bool released);
    void setUsed(bool used);

    wl_buffer *buffer() const;
    const QSize &size() const;
    int32_t stride() const;
    bool isReleased() const;
    bool isUsed() const;
    uchar *address();
private:
    wl_buffer *m_nativeBuffer;
    bool m_released;
    QSize m_size;
    int32_t m_stride;
    size_t m_offset;
    bool m_used;
};

class ShmPool : public QObject
{
    Q_OBJECT
public:
    ShmPool(wl_shm *shm);
    ~ShmPool();
    bool isValid() const;
    wl_buffer *createBuffer(const QImage &image);
    wl_buffer *createBuffer(const QSize &size, int32_t stride, const void *src);
    void *poolAddress() const;
    Buffer *getBuffer(const QSize &size, int32_t stride);
    wl_shm *shm();
Q_SIGNALS:
    void poolResized();
private:
    bool createPool();
    bool resizePool(int32_t newSize);
    wl_shm *m_shm;
    wl_shm_pool *m_pool;
    void *m_poolData;
    int32_t m_size;
    QScopedPointer<QTemporaryFile> m_tmpFile;
    bool m_valid;
    int m_offset;
    QList<Buffer*> m_buffers;
};

class WaylandSeat : public QObject
{
    Q_OBJECT
public:
    WaylandSeat(wl_seat *seat, WaylandBackend *backend);
    virtual ~WaylandSeat();

    void changed(uint32_t capabilities);
    wl_seat *seat();
    void pointerEntered(uint32_t serial);
    void resetCursor();
    void installCursorImage(wl_buffer *image, const QSize &size, const QPoint &hotspot);
    void installCursorImage(Qt::CursorShape shape);
private Q_SLOTS:
    void loadTheme();
private:
    void destroyPointer();
    void destroyKeyboard();
    void destroyTheme();
    wl_seat *m_seat;
    wl_pointer *m_pointer;
    wl_keyboard *m_keyboard;
    wl_surface *m_cursor;
    wl_cursor_theme *m_theme;
    uint32_t m_enteredSerial;
    QScopedPointer<X11CursorTracker> m_cursorTracker;
    WaylandBackend *m_backend;
};

class Output : public QObject
{
    Q_OBJECT
public:
    Output(wl_output *output, QObject *parent);
    virtual ~Output();

    wl_output *output();
    const QSize &physicalSize() const;
    const QPoint &globalPosition() const;
    const QString &manufacturer() const;
    const QString &model() const;
    const QSize &pixelSize() const;
    QRect geometry() const;
    int refreshRate() const;

    void setPhysicalSize(const QSize &size);
    void setGlobalPosition(const QPoint &pos);
    void setManufacturer(const QString &manufacturer);
    void setModel(const QString &model);
    void setPixelSize(const QSize &size);
    void setRefreshRate(int refreshRate);

    void emitChanged();

Q_SIGNALS:
    void changed();

private:
    wl_output *m_output;
    QSize m_physicalSize;
    QPoint m_globalPosition;
    QString m_manufacturer;
    QString m_model;
    QSize m_pixelSize;
    int m_refreshRate;
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
    void addOutput(wl_output *o);
    const QList<Output*> &outputs() const;
    ShmPool *shmPool();
    void createSeat(uint32_t name);
    void createShm(uint32_t name);
    void ping(uint32_t serial);

    wl_surface *surface() const;
    const QSize &shellSurfaceSize() const;
    void setShellSurfaceSize(const QSize &size);
    void installCursorImage(Qt::CursorShape shape);

    void dispatchEvents();
Q_SIGNALS:
    void shellSurfaceSizeChanged(const QSize &size);
    void systemCompositorDied();
    void backendReady();
    void outputsChanged();
private Q_SLOTS:
    void readEvents();
    void socketFileChanged(const QString &socket);
    void socketDirectoryChanged();
private:
    void initConnection();
    void createSurface();
    void destroyOutputs();
    wl_display *m_display;
    wl_registry *m_registry;
    wl_compositor *m_compositor;
    wl_shell *m_shell;
    wl_surface *m_surface;
    wl_shell_surface *m_shellSurface;
    QSize m_shellSurfaceSize;
    QScopedPointer<WaylandSeat> m_seat;
    QScopedPointer<ShmPool> m_shm;
    bool m_systemCompositorDied;
    QString m_socketName;
    QDir m_runtimeDir;
    QFileSystemWatcher *m_socketWatcher;
    QList<Output*> m_outputs;

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
void* ShmPool::poolAddress() const
{
    return m_poolData;
}

inline
wl_shm *ShmPool::shm()
{
    return m_shm;
}

inline
QRect Output::geometry() const
{
    return QRect(m_globalPosition, m_pixelSize);
}

inline
const QPoint &Output::globalPosition() const
{
    return m_globalPosition;
}

inline
const QString &Output::manufacturer() const
{
    return m_manufacturer;
}

inline
const QString &Output::model() const
{
    return m_model;
}

inline
wl_output *Output::output()
{
    return m_output;
}

inline
const QSize &Output::physicalSize() const
{
    return m_physicalSize;
}

inline
const QSize &Output::pixelSize() const
{
    return m_pixelSize;
}

inline
int Output::refreshRate() const
{
    return m_refreshRate;
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

inline
const QList< Output* >& WaylandBackend::outputs() const
{
    return m_outputs;
}

inline
wl_buffer* Buffer::buffer() const
{
    return m_nativeBuffer;
}

inline
const QSize& Buffer::size() const
{
    return m_size;
}

inline
int32_t Buffer::stride() const
{
    return m_stride;
}

inline
bool Buffer::isReleased() const
{
    return m_released;
}

inline
void Buffer::setReleased(bool released)
{
    m_released = released;
}

inline
bool Buffer::isUsed() const
{
    return m_used;
}

inline
void Buffer::setUsed(bool used)
{
    m_used = used;
}

} // namespace Wayland
} // namespace KWin

#endif //  KWIN_WAYLAND_BACKEND_H
