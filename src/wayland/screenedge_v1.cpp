/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "wayland/screenedge_v1.h"
#include "wayland/display.h"
#include "wayland/surface.h"

#include "qwayland-server-kde-screen-edge-v2.h"

#include <QPointer>

namespace KWin
{

static const int s_version = 1;

class ScreenEdgeManagerV2InterfacePrivate : public QtWaylandServer::kde_screen_edge_manager_v2
{
public:
    ScreenEdgeManagerV2InterfacePrivate(ScreenEdgeManagerV2Interface *q, Display *display);

    ScreenEdgeManagerV2Interface *q;

protected:
    void kde_screen_edge_manager_v2_destroy(Resource *resource) override;
    void kde_screen_edge_manager_v2_get_screen_edge(Resource *resource, uint32_t id, uint32_t border, struct ::wl_resource *surface) override;
};

ScreenEdgeManagerV2InterfacePrivate::ScreenEdgeManagerV2InterfacePrivate(ScreenEdgeManagerV2Interface *q, Display *display)
    : QtWaylandServer::kde_screen_edge_manager_v2(*display, s_version)
    , q(q)
{
}

void ScreenEdgeManagerV2InterfacePrivate::kde_screen_edge_manager_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ScreenEdgeManagerV2InterfacePrivate::kde_screen_edge_manager_v2_get_screen_edge(Resource *resource, uint32_t id, uint32_t border, struct ::wl_resource *surface_resource)
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

    wl_resource *edgeResource = wl_resource_create(resource->client(), &kde_screen_edge_v2_interface, resource->version(), id);
    auto edge = new ScreenEdgeV2Interface(surface, electricBorder, edgeResource);
    Q_EMIT q->edgeRequested(edge);
}

ScreenEdgeManagerV2Interface::ScreenEdgeManagerV2Interface(Display *display, QObject *parent)
    : d(new ScreenEdgeManagerV2InterfacePrivate(this, display))
{
}

ScreenEdgeManagerV2Interface::~ScreenEdgeManagerV2Interface()
{
}

class ScreenEdgeV2InterfacePrivate : public QtWaylandServer::kde_screen_edge_v2
{
public:
    ScreenEdgeV2InterfacePrivate(ScreenEdgeV2Interface *q, SurfaceInterface *surface, ElectricBorder border, wl_resource *resource);

    ScreenEdgeV2Interface *q;
    QPointer<SurfaceInterface> surface;
    ElectricBorder border;

protected:
    void kde_screen_edge_v2_destroy_resource(Resource *resource) override;
    void kde_screen_edge_v2_destroy(Resource *resource) override;
    void kde_screen_edge_v2_show(Resource *resource) override;
    void kde_screen_edge_v2_hide(Resource *resource) override;
};

ScreenEdgeV2InterfacePrivate::ScreenEdgeV2InterfacePrivate(ScreenEdgeV2Interface *q, SurfaceInterface *surface, ElectricBorder border, wl_resource *resource)
    : QtWaylandServer::kde_screen_edge_v2(resource)
    , q(q)
    , surface(surface)
    , border(border)
{
}

void ScreenEdgeV2InterfacePrivate::kde_screen_edge_v2_destroy_resource(Resource *resource)
{
    delete q;
}

void ScreenEdgeV2InterfacePrivate::kde_screen_edge_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ScreenEdgeV2InterfacePrivate::kde_screen_edge_v2_show(Resource *resource)
{
    Q_EMIT q->showRequested();
}

void ScreenEdgeV2InterfacePrivate::kde_screen_edge_v2_hide(Resource *resource)
{
    Q_EMIT q->hideRequested();
}

ScreenEdgeV2Interface::ScreenEdgeV2Interface(SurfaceInterface *surface, ElectricBorder border, wl_resource *resource)
    : d(new ScreenEdgeV2InterfacePrivate(this, surface, border, resource))
{
}

ScreenEdgeV2Interface::~ScreenEdgeV2Interface()
{
}

SurfaceInterface *ScreenEdgeV2Interface::surface() const
{
    return d->surface;
}

ElectricBorder ScreenEdgeV2Interface::border() const
{
    return d->border;
}

} // namespace KWin

#include "moc_screenedge_v1.cpp"
