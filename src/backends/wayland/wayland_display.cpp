/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wayland_display.h"
#include "wayland_logging.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/pointerconstraints.h>
#include <KWayland/Client/pointergestures.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/relativepointer.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/subcompositor.h>
#include <KWayland/Client/xdgshell.h>

#include <QMutex>
#include <QThread>
#include <QWaitCondition>

#include <poll.h>
#include <unistd.h>
#include <wayland-client.h>

// Generated in src/wayland.
#include "wayland-pointer-constraints-unstable-v1-client-protocol.h"
#include "wayland-pointer-gestures-unstable-v1-server-protocol.h"
#include "wayland-relative-pointer-unstable-v1-client-protocol.h"
#include "wayland-server-decoration-client-protocol.h"
#include "wayland-xdg-shell-client-protocol.h"

namespace KWin
{
namespace Wayland
{

class WaylandEventThread : public QThread
{
    Q_OBJECT

public:
    WaylandEventThread(wl_display *display)
        : m_display(display)
        , m_fd(wl_display_get_fd(display))
        , m_quitPipe{-1, -1}
        , m_reading(true)
        , m_quitting(false)
    {
        if (pipe(m_quitPipe) == -1) {
            qCWarning(KWIN_WAYLAND_BACKEND) << "Failed to create quite pipe in WaylandEventThread";
        }
    }

    ~WaylandEventThread() override
    {
        if (m_quitPipe[0] != -1) {
            close(m_quitPipe[0]);
            close(m_quitPipe[1]);
        }
    }

    void dispatch()
    {
        while (true) {
            if (wl_display_dispatch_pending(m_display) < 0) {
                qFatal("Wayland connection broke");
            }

            wl_display_flush(m_display);

            if (m_reading.loadAcquire()) {
                break;
            }

            if (wl_display_prepare_read(m_display) == 0) {
                QMutexLocker lock(&m_mutex);
                m_reading.storeRelease(true);
                m_cond.wakeOne();
                break;
            }
        }
    }

    void stop()
    {
        if (m_quitPipe[1] != -1) {
            write(m_quitPipe[1], "\0", 1);
        }

        m_mutex.lock();
        m_quitting = true;
        m_cond.wakeOne();
        m_mutex.unlock();

        wait();
    }

Q_SIGNALS:
    void available();

protected:
    void run() override
    {
        while (true) {
            m_reading.storeRelease(false);

            Q_EMIT available();

            m_mutex.lock();
            while (!m_reading.loadRelaxed() && !m_quitting) {
                m_cond.wait(&m_mutex);
            }
            m_mutex.unlock();

            if (m_quitting) {
                break;
            }

            pollfd fds[2] = { { m_fd, POLLIN, 0 }, { m_quitPipe[0], POLLIN, 0 } };
            poll(fds, 2, -1);

            if (fds[1].revents & POLLIN) {
                wl_display_cancel_read(m_display);
                break;
            }

            if (fds[0].revents & POLLIN) {
                wl_display_read_events(m_display);
            } else {
                wl_display_cancel_read(m_display);
            }
        }
    }

private:
    wl_display *const m_display;
    int m_fd;
    int m_quitPipe[2];
    QAtomicInteger<bool> m_reading;
    QMutex m_mutex;
    QWaitCondition m_cond;
    bool m_quitting;
};

WaylandDisplay::WaylandDisplay()
{
}

WaylandDisplay::~WaylandDisplay()
{
    m_eventThread->stop();
    m_eventThread.reset();

    m_compositor.reset();
    m_pointerConstraints.reset();
    m_pointerGestures.reset();
    m_relativePointerManager.reset();
    m_seat.reset();
    m_serverSideDecorationManager.reset();
    m_shmPool.reset();
    m_subCompositor.reset();
    m_xdgShell.reset();

    if (m_registry) {
        wl_registry_destroy(m_registry);
    }
    if (m_display) {
        wl_display_disconnect(m_display);
    }
}

void WaylandDisplay::flush()
{
    m_eventThread->dispatch();
}

bool WaylandDisplay::initialize(const QString &socketName)
{
    m_display = wl_display_connect(socketName.toUtf8());
    if (!m_display) {
        return false;
    }

    m_eventThread = std::make_unique<WaylandEventThread>(m_display);
    connect(m_eventThread.get(), &WaylandEventThread::available, this, &WaylandDisplay::flush, Qt::QueuedConnection);
    m_eventThread->start();

    static wl_registry_listener registryListener {
        .global = registry_global,
        .global_remove = registry_global_remove,
    };
    m_registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(m_registry, &registryListener, this);
    wl_display_roundtrip(m_display);

    return true;
}

wl_display *WaylandDisplay::nativeDisplay() const
{
    return m_display;
}

KWayland::Client::Compositor *WaylandDisplay::compositor() const
{
    return m_compositor.get();
}

KWayland::Client::PointerConstraints *WaylandDisplay::pointerConstraints() const
{
    return m_pointerConstraints.get();
}

KWayland::Client::PointerGestures *WaylandDisplay::pointerGestures() const
{
    return m_pointerGestures.get();
}

KWayland::Client::RelativePointerManager *WaylandDisplay::relativePointerManager() const
{
    return m_relativePointerManager.get();
}

KWayland::Client::ShmPool *WaylandDisplay::shmPool() const
{
    return m_shmPool.get();
}

KWayland::Client::SubCompositor *WaylandDisplay::subCompositor() const
{
    return m_subCompositor.get();
}

KWayland::Client::Seat *WaylandDisplay::seat() const
{
    return m_seat.get();
}

KWayland::Client::XdgShell *WaylandDisplay::xdgShell() const
{
    return m_xdgShell.get();
}

KWayland::Client::ServerSideDecorationManager *WaylandDisplay::serverSideDecorationManager() const
{
    return m_serverSideDecorationManager.get();
}

void WaylandDisplay::registry_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    WaylandDisplay *display = static_cast<WaylandDisplay *>(data);

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        if (version < 4) {
            qFatal("wl_compositor version 4 or later is required");
        }
        display->m_compositor = std::make_unique<KWayland::Client::Compositor>();
        display->m_compositor->setup(static_cast<wl_compositor *>(wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4u))));
    } else if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
        display->m_subCompositor = std::make_unique<KWayland::Client::SubCompositor>();
        display->m_subCompositor->setup(static_cast<wl_subcompositor *>(wl_registry_bind(registry, name, &wl_subcompositor_interface, std::min(version, 1u))));
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        display->m_shmPool = std::make_unique<KWayland::Client::ShmPool>();
        display->m_shmPool->setup(static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, std::min(version, 1u))));
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        display->m_seat = std::make_unique<KWayland::Client::Seat>();
        display->m_seat->setup(static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 2u))));
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        display->m_xdgShell = std::make_unique<KWayland::Client::XdgShellStable>();
        display->m_xdgShell->setup(static_cast<xdg_wm_base *>(wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min(version, 1u))));
    } else if (strcmp(interface, zwp_pointer_constraints_v1_interface.name) == 0) {
        display->m_pointerConstraints = std::make_unique<KWayland::Client::PointerConstraints>();
        display->m_pointerConstraints->setup(static_cast<zwp_pointer_constraints_v1 *>(wl_registry_bind(registry, name, &zwp_pointer_constraints_v1_interface, std::min(version, 1u))));
    } else if (strcmp(interface, zwp_pointer_gestures_v1_interface.name) == 0) {
        display->m_pointerGestures = std::make_unique<KWayland::Client::PointerGestures>();
        display->m_pointerGestures->setup(static_cast<zwp_pointer_gestures_v1 *>(wl_registry_bind(registry, name, &zwp_pointer_gestures_v1_interface, std::min(version, 1u))));
    } else if (strcmp(interface, zwp_relative_pointer_manager_v1_interface.name) == 0) {
        display->m_relativePointerManager = std::make_unique<KWayland::Client::RelativePointerManager>();
        display->m_relativePointerManager->setup(static_cast<zwp_relative_pointer_manager_v1 *>(wl_registry_bind(registry, name, &zwp_relative_pointer_manager_v1_interface, std::min(version, 1u))));
    } else if (strcmp(interface, org_kde_kwin_server_decoration_manager_interface.name) == 0) {
        display->m_serverSideDecorationManager = std::make_unique<KWayland::Client::ServerSideDecorationManager>();
        display->m_serverSideDecorationManager->setup(static_cast<org_kde_kwin_server_decoration_manager *>(wl_registry_bind(registry, name, &org_kde_kwin_server_decoration_manager_interface, std::min(version, 1u))));
    }
}

void WaylandDisplay::registry_global_remove(void *data, wl_registry *registry, uint32_t name)
{
}

}
}

#include "wayland_display.moc"
