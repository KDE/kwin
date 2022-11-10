/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

#include <memory>

struct wl_display;
struct wl_registry;

namespace KWayland
{
namespace Client
{
class Compositor;
class PointerConstraints;
class PointerGestures;
class RelativePointerManager;
class Seat;
class ServerSideDecorationManager;
class ShmPool;
class SubCompositor;
class XdgShell;
}
}

namespace KWin
{
namespace Wayland
{

class WaylandEventThread;

class WaylandDisplay : public QObject
{
    Q_OBJECT

public:
    WaylandDisplay();
    ~WaylandDisplay() override;

    bool initialize(const QString &socketName);

    wl_display *nativeDisplay() const;
    KWayland::Client::Compositor *compositor() const;
    KWayland::Client::PointerConstraints *pointerConstraints() const;
    KWayland::Client::PointerGestures *pointerGestures() const;
    KWayland::Client::RelativePointerManager *relativePointerManager() const;
    KWayland::Client::Seat *seat() const;
    KWayland::Client::ServerSideDecorationManager *serverSideDecorationManager() const;
    KWayland::Client::ShmPool *shmPool() const;
    KWayland::Client::SubCompositor *subCompositor() const;
    KWayland::Client::XdgShell *xdgShell() const;

public Q_SLOTS:
    void flush();

private:
    static void registry_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    static void registry_global_remove(void *data, wl_registry *registry, uint32_t name);

    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    std::unique_ptr<WaylandEventThread> m_eventThread;
    std::unique_ptr<KWayland::Client::Compositor> m_compositor;
    std::unique_ptr<KWayland::Client::PointerConstraints> m_pointerConstraints;
    std::unique_ptr<KWayland::Client::PointerGestures> m_pointerGestures;
    std::unique_ptr<KWayland::Client::RelativePointerManager> m_relativePointerManager;
    std::unique_ptr<KWayland::Client::Seat> m_seat;
    std::unique_ptr<KWayland::Client::ServerSideDecorationManager> m_serverSideDecorationManager;
    std::unique_ptr<KWayland::Client::ShmPool> m_shmPool;
    std::unique_ptr<KWayland::Client::SubCompositor> m_subCompositor;
    std::unique_ptr<KWayland::Client::XdgShell> m_xdgShell;
};

}
}
