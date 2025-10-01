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
struct wl_shm;
struct zwp_linux_dmabuf_v1;
struct wp_presentation;
struct wp_tearing_control_manager_v1;
struct wp_fractional_scale_manager_v1;
struct wp_viewporter;
struct wp_single_pixel_buffer_manager_v1;

namespace KWayland
{
namespace Client
{
class Compositor;
class PointerConstraints;
class PointerGestures;
class RelativePointerManager;
class Seat;
class XdgDecorationManager;
class XdgShell;
class SubCompositor;
}
}

namespace KWin::WaylandClient
{
class LinuxDmabufV1;
}

namespace KWin
{
namespace Wayland
{

class WaylandEventThread;
class WaylandLinuxDmabufFeedbackV1;
class ColorManager;

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
    KWayland::Client::SubCompositor *subCompositor() const;
    wl_shm *shm() const;
    KWayland::Client::XdgShell *xdgShell() const;
    WaylandClient::LinuxDmabufV1 *linuxDmabuf() const;
    wp_presentation *presentationTime() const;
    wp_tearing_control_manager_v1 *tearingControl() const;
    ColorManager *colorManager() const;
    wp_fractional_scale_manager_v1 *fractionalScale() const;
    wp_viewporter *viewporter() const;
    wp_single_pixel_buffer_manager_v1 *singlePixelManager() const;

public Q_SLOTS:
    void flush();

private:
    static void registry_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    static void registry_global_remove(void *data, wl_registry *registry, uint32_t name);

    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    wl_shm *m_shm = nullptr;
    wp_presentation *m_presentationTime = nullptr;
    wp_tearing_control_manager_v1 *m_tearingControl = nullptr;
    wp_fractional_scale_manager_v1 *m_fractionalScaleV1 = nullptr;
    wp_viewporter *m_viewporter = nullptr;
    wp_single_pixel_buffer_manager_v1 *m_singlePixelManager = nullptr;
    std::unique_ptr<ColorManager> m_colorManager;
    std::unique_ptr<WaylandEventThread> m_eventThread;
    std::unique_ptr<WaylandClient::LinuxDmabufV1> m_linuxDmabuf;
    std::unique_ptr<KWayland::Client::Compositor> m_compositor;
    std::unique_ptr<KWayland::Client::SubCompositor> m_subCompositor;
    std::unique_ptr<KWayland::Client::PointerConstraints> m_pointerConstraints;
    std::unique_ptr<KWayland::Client::PointerGestures> m_pointerGestures;
    std::unique_ptr<KWayland::Client::RelativePointerManager> m_relativePointerManager;
    std::unique_ptr<KWayland::Client::Seat> m_seat;
    std::unique_ptr<KWayland::Client::XdgDecorationManager> m_xdgDecorationManager;
    std::unique_ptr<KWayland::Client::XdgShell> m_xdgShell;
};

}
}
