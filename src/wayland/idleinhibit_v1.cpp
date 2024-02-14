/*
    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "display.h"
#include "idleinhibit_v1_p.h"
#include "surface_p.h"

namespace KWin
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

void IdleInhibitManagerV1InterfacePrivate::zwp_idle_inhibit_manager_v1_create_inhibitor(Resource *resource, uint32_t id, wl_resource *wlsurface)
{
    const auto surface = SurfaceInterface::get(wlsurface);
    const auto inhibitor = new IdleInhibitorV1Interface(resource->client(), id, resource->version(), surface);
    SurfaceInterfacePrivate::get(surface)->installIdleInhibitor(inhibitor);
}

IdleInhibitManagerV1Interface::IdleInhibitManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new IdleInhibitManagerV1InterfacePrivate(this, display))
{
}

IdleInhibitManagerV1Interface::~IdleInhibitManagerV1Interface() = default;

IdleInhibitorV1Interface::IdleInhibitorV1Interface(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface)
    : QtWaylandServer::zwp_idle_inhibitor_v1(client, id, version)
    , m_surface(surface)
{
}

IdleInhibitorV1Interface::~IdleInhibitorV1Interface()
{
    if (m_surface) {
        SurfaceInterfacePrivate::get(m_surface)->removeIdleInhibitor(this);
    }
}

void IdleInhibitorV1Interface::zwp_idle_inhibitor_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void IdleInhibitorV1Interface::zwp_idle_inhibitor_v1_destroy_resource(Resource *resource)
{
    delete this;
}

}

#include "moc_idleinhibit_v1.cpp"
#include "moc_idleinhibit_v1_p.cpp"
