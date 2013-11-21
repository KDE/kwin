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
#ifndef KWIN_EGL_WAYLAND_BACKEND_H
#define KWIN_EGL_WAYLAND_BACKEND_H
#include "scene_opengl.h"
// wayland
#include <wayland-client.h>
#include <wayland-egl.h>
// xcb
#include <xcb/shm.h>

class KTemporaryFile;
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
    QScopedPointer<KTemporaryFile> m_tmpFile;
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
* the Wayland surface and it's shell and egl mapping.
*/
class WaylandBackend : public QObject
{
    Q_OBJECT
public:
    WaylandBackend();
    virtual ~WaylandBackend();
    wl_display *display();
    wl_registry *registry();
    void setCompositor(wl_compositor *c);
    wl_compositor *compositor();
    void setShell(wl_shell *s);
    wl_shell *shell();
    wl_egl_window *overlay();
    ShmPool *shmPool();
    void createSeat(uint32_t name);
    void createShm(uint32_t name);
    void ping(uint32_t serial);

    bool createSurface();
private Q_SLOTS:
    void readEvents();
private:
    wl_display *m_display;
    wl_registry *m_registry;
    wl_compositor *m_compositor;
    wl_shell *m_shell;
    wl_surface *m_surface;
    wl_egl_window *m_overlay;
    wl_shell_surface *m_shellSurface;
    QScopedPointer<WaylandSeat> m_seat;
    QScopedPointer<ShmPool> m_shm;
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
wl_egl_window *WaylandBackend::overlay()
{
    return m_overlay;
}

inline
void WaylandBackend::setShell(wl_shell *s)
{
    m_shell = s;
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

} // namespace Wayland

class Shm;

/**
 * @brief OpenGL Backend using Egl on a Wayland surface.
 *
 * This Backend is the basis for a session compositor running on top of a Wayland system compositor.
 * It creates a Surface as large as the screen and maps it as a fullscreen shell surface on the
 * system compositor. The OpenGL context is created on the Wayland surface, so for rendering X11 is
 * not involved.
 *
 * At the moment the backend is still rather limited. For getting textures from pixmap it uses the
 * XShm library. This is currently a hack and only as proof of concept till we support texture from
 * Wayland buffers. From then on we should use XWayland for texture mapping.
 *
 * Also in repainting the backend is currently still rather limited. Only supported mode is fullscreen
 * repaints, which is obviously not optimal. Best solution is probably to go for buffer_age extension
 * and make it the only available solution next to fullscreen repaints.
 **/
class EglWaylandBackend : public OpenGLBackend
{
public:
    EglWaylandBackend();
    virtual ~EglWaylandBackend();
    virtual void screenGeometryChanged(const QSize &size);
    virtual SceneOpenGL::TexturePrivate *createBackendTexture(SceneOpenGL::Texture *texture);
    virtual QRegion prepareRenderingFrame();
    virtual void endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion);
    Shm *shm();

protected:
    virtual void present();

private:
    void init();
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();
    bool makeContextCurrent();
    EGLDisplay m_display;
    EGLConfig m_config;
    EGLSurface m_surface;
    EGLContext m_context;
    QScopedPointer<Wayland::WaylandBackend> m_wayland;
    QScopedPointer<Shm> m_shm;
    friend class EglWaylandTexture;
};

/**
 * @brief Texture using an EGLImageKHR.
 **/
class EglWaylandTexture : public SceneOpenGL::TexturePrivate
{
public:
    virtual ~EglWaylandTexture();
    virtual void findTarget();
    virtual bool loadTexture(const Pixmap& pix, const QSize& size, int depth);
    virtual OpenGLBackend *backend();
    virtual bool update(const QRegion &damage);

private:
    friend class EglWaylandBackend;
    EglWaylandTexture(SceneOpenGL::Texture *texture, EglWaylandBackend *backend);
    SceneOpenGL::Texture *q;
    EglWaylandBackend *m_backend;
    /**
     * The Pixmap of the window content. Get's updated in loadTexture.
     */
    xcb_pixmap_t m_referencedPixmap;
};

/**
 * @brief Small helper class to encapsulate SHM related functionality.
 *
 */
class Shm
{
public:
    Shm();
    ~Shm();
    int shmId() const;
    void *buffer() const;
    xcb_shm_seg_t segment() const;
    bool isValid() const;
private:
    bool init();
    int m_shmId;
    void *m_buffer;
    xcb_shm_seg_t m_segment;
    bool m_valid;
};

inline
void *Shm::buffer() const
{
    return m_buffer;
}

inline
bool Shm::isValid() const
{
    return m_valid;
}

inline
xcb_shm_seg_t Shm::segment() const
{
    return m_segment;
}

inline
int Shm::shmId() const
{
    return m_shmId;
}

} // namespace

#endif //  KWIN_EGL_ON_X_BACKEND_H
