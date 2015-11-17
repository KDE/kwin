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
#include "abstract_backend.h"
#include <config-kwin.h>
#include <kwinglobals.h>
// Qt
#include <QHash>
#include <QImage>
#include <QObject>
#include <QPoint>
#include <QSize>

class QTemporaryFile;
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
class Keyboard;
class Pointer;
class Registry;
class Seat;
class Shell;
class ShellSurface;
class Surface;
class Touch;
}
}

namespace KWin
{
class WaylandCursorTheme;

namespace Wayland
{

class WaylandBackend;
class WaylandSeat;

class WaylandSeat : public QObject
{
    Q_OBJECT
public:
    WaylandSeat(wl_seat *seat, WaylandBackend *backend);
    virtual ~WaylandSeat();

    void installCursorImage(wl_buffer *image, const QSize &size, const QPoint &hotspot);
    void installCursorImage(Qt::CursorShape shape);
    void installCursorImage(const QImage &image, const QPoint &hotspot);
    void setInstallCursor(bool install);
    bool isInstallCursor() const {
        return m_installCursor;
    }
private:
    void destroyPointer();
    void destroyKeyboard();
    void destroyTouch();
    KWayland::Client::Seat *m_seat;
    KWayland::Client::Pointer *m_pointer;
    KWayland::Client::Keyboard *m_keyboard;
    KWayland::Client::Touch *m_touch;
    KWayland::Client::Surface *m_cursor;
    WaylandCursorTheme *m_theme = nullptr;
    uint32_t m_enteredSerial;
    WaylandBackend *m_backend;
    bool m_installCursor;
};

/**
* @brief Class encapsulating all Wayland data structures needed by the Egl backend.
*
* It creates the connection to the Wayland Compositor, sets up the registry and creates
* the Wayland surface and its shell mapping.
*/
class KWIN_EXPORT WaylandBackend : public AbstractBackend
{
    Q_OBJECT
    Q_INTERFACES(KWin::AbstractBackend)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.AbstractBackend" FILE "wayland.json")
public:
    explicit WaylandBackend(QObject *parent = nullptr);
    virtual ~WaylandBackend();
    void init() override;
    wl_display *display();
    KWayland::Client::Compositor *compositor();
    KWayland::Client::ShmPool *shmPool();

    KWayland::Client::Surface *surface() const;
    QSize shellSurfaceSize() const;
    void installCursorImage(Qt::CursorShape shape) override;
    void installCursorFromServer() override;

    Screens *createScreens(QObject *parent = nullptr) override;
    OpenGLBackend *createOpenGLBackend() override;
    QPainterBackend *createQPainterBackend() override;

    QSize screenSize() const override {
        return shellSurfaceSize();
    }

Q_SIGNALS:
    void shellSurfaceSizeChanged(const QSize &size);
    void systemCompositorDied();
    void connectionFailed();
private:
    void initConnection();
    void createSurface();
    wl_display *m_display;
    KWayland::Client::EventQueue *m_eventQueue;
    KWayland::Client::Registry *m_registry;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::Shell *m_shell;
    KWayland::Client::Surface *m_surface;
    KWayland::Client::ShellSurface *m_shellSurface;
    QScopedPointer<WaylandSeat> m_seat;
    KWayland::Client::ShmPool *m_shm;
    KWayland::Client::ConnectionThread *m_connectionThreadObject;
    QThread *m_connectionThread;
};

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

} // namespace Wayland
} // namespace KWin

#endif //  KWIN_WAYLAND_BACKEND_H
