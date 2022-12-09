/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHash>
#include <QObject>

#include <memory>

struct wl_display;
struct wl_registry;
struct zwp_linux_dmabuf_v1;

namespace KWayland
{
namespace Client
{
class Compositor;
class PointerConstraints;
class PointerGestures;
class RelativePointerManager;
class Seat;
class ShmPool;
class XdgDecorationManager;
class XdgShell;
}
}

namespace KWin
{
namespace Wayland
{

class WaylandEventThread;

class WaylandLinuxDmabufV1
{
public:
    WaylandLinuxDmabufV1(wl_registry *registry, uint32_t name, uint32_t version);
    ~WaylandLinuxDmabufV1();

    zwp_linux_dmabuf_v1 *handle() const;
    QHash<uint32_t, QVector<uint64_t>> formats() const;

private:
    static void format(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf_v1, uint32_t format);
    static void modifier(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf_v1, uint32_t format, uint32_t modifier_hi, uint32_t modifier_lo);

    zwp_linux_dmabuf_v1 *m_dmabuf;
    QHash<uint32_t, QVector<uint64_t>> m_formats;
};

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
    KWayland::Client::XdgDecorationManager *xdgDecorationManager() const;
    KWayland::Client::ShmPool *shmPool() const;
    KWayland::Client::XdgShell *xdgShell() const;
    WaylandLinuxDmabufV1 *linuxDmabuf() const;

public Q_SLOTS:
    void flush();

private:
    static void registry_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    static void registry_global_remove(void *data, wl_registry *registry, uint32_t name);

    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    std::unique_ptr<WaylandEventThread> m_eventThread;
    std::unique_ptr<WaylandLinuxDmabufV1> m_linuxDmabuf;
    std::unique_ptr<KWayland::Client::Compositor> m_compositor;
    std::unique_ptr<KWayland::Client::PointerConstraints> m_pointerConstraints;
    std::unique_ptr<KWayland::Client::PointerGestures> m_pointerGestures;
    std::unique_ptr<KWayland::Client::RelativePointerManager> m_relativePointerManager;
    std::unique_ptr<KWayland::Client::Seat> m_seat;
    std::unique_ptr<KWayland::Client::XdgDecorationManager> m_xdgDecorationManager;
    std::unique_ptr<KWayland::Client::ShmPool> m_shmPool;
    std::unique_ptr<KWayland::Client::XdgShell> m_xdgShell;
};

}
}
