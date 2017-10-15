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
#include "platform.h"
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
class ConfinedPointer;
class ConnectionThread;
class EventQueue;
class Keyboard;
class Pointer;
class PointerConstraints;
class PointerGestures;
class PointerSwipeGesture;
class PointerPinchGesture;
class Registry;
class Seat;
class Shell;
class ShellSurface;
class Surface;
class Touch;
class XdgShell;
class XdgShellSurface;
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
    void installCursorImage(const QImage &image, const QPoint &hotspot);
    void setInstallCursor(bool install);
    bool isInstallCursor() const {
        return m_installCursor;
    }

    KWayland::Client::Pointer *pointer() const {
        return m_pointer;
    }

    void installGesturesInterface(KWayland::Client::PointerGestures *gesturesInterface) {
        m_gesturesInterface = gesturesInterface;
        setupPointerGestures();
    }

private:
    void destroyPointer();
    void destroyKeyboard();
    void destroyTouch();
    void setupPointerGestures();
    KWayland::Client::Seat *m_seat;
    KWayland::Client::Pointer *m_pointer;
    KWayland::Client::Keyboard *m_keyboard;
    KWayland::Client::Touch *m_touch;
    KWayland::Client::Surface *m_cursor;
    KWayland::Client::PointerGestures *m_gesturesInterface = nullptr;
    KWayland::Client::PointerPinchGesture *m_pinchGesture = nullptr;
    KWayland::Client::PointerSwipeGesture *m_swipeGesture = nullptr;
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
class KWIN_EXPORT WaylandBackend : public Platform
{
    Q_OBJECT
    Q_INTERFACES(KWin::Platform)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Platform" FILE "wayland.json")
public:
    explicit WaylandBackend(QObject *parent = nullptr);
    virtual ~WaylandBackend();
    void init() override;
    wl_display *display();
    KWayland::Client::Compositor *compositor();
    KWayland::Client::ShmPool *shmPool();

    KWayland::Client::Surface *surface() const;
    QSize shellSurfaceSize() const;

    Screens *createScreens(QObject *parent = nullptr) override;
    OpenGLBackend *createOpenGLBackend() override;
    QPainterBackend *createQPainterBackend() override;

    QSize screenSize() const override {
        return shellSurfaceSize();
    }

    void flush();

    void togglePointerConfinement();

    QVector<CompositingType> supportedCompositors() const override;

Q_SIGNALS:
    void shellSurfaceSizeChanged(const QSize &size);
    void systemCompositorDied();
    void connectionFailed();
private:
    void initConnection();
    void createSurface();
    template <class T>
    void setupSurface(T *surface);
    void updateWindowTitle();
    wl_display *m_display;
    KWayland::Client::EventQueue *m_eventQueue;
    KWayland::Client::Registry *m_registry;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::Shell *m_shell;
    KWayland::Client::Surface *m_surface;
    KWayland::Client::ShellSurface *m_shellSurface;
    KWayland::Client::XdgShell *m_xdgShell = nullptr;
    KWayland::Client::XdgShellSurface *m_xdgShellSurface = nullptr;
    QScopedPointer<WaylandSeat> m_seat;
    KWayland::Client::ShmPool *m_shm;
    KWayland::Client::ConnectionThread *m_connectionThreadObject;
    KWayland::Client::PointerConstraints *m_pointerConstraints = nullptr;
    KWayland::Client::ConfinedPointer *m_pointerConfinement = nullptr;
    QThread *m_connectionThread;
    bool m_isPointerConfined = false;
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
