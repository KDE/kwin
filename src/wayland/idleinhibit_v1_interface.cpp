/*
    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "display.h"
#include "idleinhibit_v1_interface_p.h"
#include "surface_interface_p.h"

namespace KWaylandServer
{
static const quint32 s_version = 1;

IdleInhibitManagerV1InterfacePrivate::IdleInhibitManagerV1InterfacePrivate(IdleInhibitManagerV1Interface *_q, Display *display)
    : QtWaylandServer::zwp_idle_inhibit_manager_v1(*display, s_version)
    , q(_q)
{
}

void IdleInhibitManagerV1InterfacePrivate::zwp_idle_inhibit_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void IdleInhibitManagerV1InterfacePrivate::zwp_idle_inhibit_manager_v1_create_inhibitor(Resource *resource, uint32_t id, wl_resource *surface)
{
    auto s = SurfaceInterface::get(surface);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "Invalid  surface");
        return;
    }

    wl_resource *inhibitorResource = wl_resource_create(resource->client(), &zwp_idle_inhibitor_v1_interface, resource->version(), id);
    if (!inhibitorResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }
    auto inhibitor = new IdleInhibitorV1Interface(inhibitorResource);

    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(s);
    surfacePrivate->installIdleInhibitor(inhibitor);
}

IdleInhibitManagerV1Interface::IdleInhibitManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new IdleInhibitManagerV1InterfacePrivate(this, display))
{
}

IdleInhibitManagerV1Interface::~IdleInhibitManagerV1Interface() = default;

IdleInhibitorV1Interface::IdleInhibitorV1Interface(wl_resource *resource)
    : QObject(nullptr)
    , QtWaylandServer::zwp_idle_inhibitor_v1(resource)
{
}

IdleInhibitorV1Interface::~IdleInhibitorV1Interface() = default;

void IdleInhibitorV1Interface::zwp_idle_inhibitor_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void IdleInhibitorV1Interface::zwp_idle_inhibitor_v1_destroy_resource(Resource *resource)
{
    delete this;
}

}
