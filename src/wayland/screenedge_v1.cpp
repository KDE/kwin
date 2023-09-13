/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "wayland/screenedge_v1.h"
#include "wayland/display.h"
#include "wayland/surface.h"

#include "qwayland-server-kde-screen-edge-v1.h"

#include <QPointer>

using namespace KWin;

namespace KWaylandServer
{

static const int s_version = 1;

class ScreenEdgeManagerV1InterfacePrivate : public QtWaylandServer::kde_screen_edge_manager_v1
{
public:
    ScreenEdgeManagerV1InterfacePrivate(ScreenEdgeManagerV1Interface *q, Display *display);

    ScreenEdgeManagerV1Interface *q;

protected:
    void kde_screen_edge_manager_v1_destroy(Resource *resource) override;
    void kde_screen_edge_manager_v1_get_auto_hide_screen_edge(Resource *resource, uint32_t id, uint32_t border, struct ::wl_resource *surface) override;
};

ScreenEdgeManagerV1InterfacePrivate::ScreenEdgeManagerV1InterfacePrivate(ScreenEdgeManagerV1Interface *q, Display *display)
    : QtWaylandServer::kde_screen_edge_manager_v1(*display, s_version)
    , q(q)
{
}

void ScreenEdgeManagerV1InterfacePrivate::kde_screen_edge_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ScreenEdgeManagerV1InterfacePrivate::kde_screen_edge_manager_v1_get_auto_hide_screen_edge(Resource *resource, uint32_t id, uint32_t border, struct ::wl_resource *surface_resource)
{
    ElectricBorder electricBorder;
    switch (border) {
    case border_top:
        electricBorder = ElectricTop;
        break;
    case border_bottom:
        electricBorder = ElectricBottom;
        break;
    case border_left:
        electricBorder = ElectricLeft;
        break;
    case border_right:
        electricBorder = ElectricRight;
        break;
    default:
        wl_resource_post_error(resource->handle, error_invalid_border, "invalid border");
        return;
    }

    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);
    const SurfaceRole *role = surface->role();
    if (!role || role->name() != "layer_surface_v1") {
        wl_resource_post_error(resource->handle, error_invalid_role, "surface must have layer_surface role");
        return;
    }

    wl_resource *edgeResource = wl_resource_create(resource->client(), &kde_auto_hide_screen_edge_v1_interface, resource->version(), id);
    auto edge = new AutoHideScreenEdgeV1Interface(surface, electricBorder, edgeResource);
    Q_EMIT q->edgeRequested(edge);
}

ScreenEdgeManagerV1Interface::ScreenEdgeManagerV1Interface(Display *display, QObject *parent)
    : d(new ScreenEdgeManagerV1InterfacePrivate(this, display))
{
}

ScreenEdgeManagerV1Interface::~ScreenEdgeManagerV1Interface()
{
}

class AutoHideScreenEdgeV1InterfacePrivate : public QtWaylandServer::kde_auto_hide_screen_edge_v1
{
public:
    AutoHideScreenEdgeV1InterfacePrivate(AutoHideScreenEdgeV1Interface *q, SurfaceInterface *surface, ElectricBorder border, wl_resource *resource);

    AutoHideScreenEdgeV1Interface *q;
    QPointer<SurfaceInterface> surface;
    ElectricBorder border;

protected:
    void kde_auto_hide_screen_edge_v1_destroy_resource(Resource *resource) override;
    void kde_auto_hide_screen_edge_v1_destroy(Resource *resource) override;
    void kde_auto_hide_screen_edge_v1_deactivate(Resource *resource) override;
    void kde_auto_hide_screen_edge_v1_activate(Resource *resource) override;
};

AutoHideScreenEdgeV1InterfacePrivate::AutoHideScreenEdgeV1InterfacePrivate(AutoHideScreenEdgeV1Interface *q, SurfaceInterface *surface, ElectricBorder border, wl_resource *resource)
    : QtWaylandServer::kde_auto_hide_screen_edge_v1(resource)
    , q(q)
    , surface(surface)
    , border(border)
{
}

void AutoHideScreenEdgeV1InterfacePrivate::kde_auto_hide_screen_edge_v1_destroy_resource(Resource *resource)
{
    delete q;
}

void AutoHideScreenEdgeV1InterfacePrivate::kde_auto_hide_screen_edge_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void AutoHideScreenEdgeV1InterfacePrivate::kde_auto_hide_screen_edge_v1_deactivate(Resource *resource)
{
    Q_EMIT q->deactivateRequested();
}

void AutoHideScreenEdgeV1InterfacePrivate::kde_auto_hide_screen_edge_v1_activate(Resource *resource)
{
    Q_EMIT q->activateRequested();
}

AutoHideScreenEdgeV1Interface::AutoHideScreenEdgeV1Interface(SurfaceInterface *surface, ElectricBorder border, wl_resource *resource)
    : d(new AutoHideScreenEdgeV1InterfacePrivate(this, surface, border, resource))
{
}

AutoHideScreenEdgeV1Interface::~AutoHideScreenEdgeV1Interface()
{
}

SurfaceInterface *AutoHideScreenEdgeV1Interface::surface() const
{
    return d->surface;
}

ElectricBorder AutoHideScreenEdgeV1Interface::border() const
{
    return d->border;
}

} // namespace KWaylandServer

#include "moc_screenedge_v1.cpp"
