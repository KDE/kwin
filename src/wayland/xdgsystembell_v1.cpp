/*
    SPDX-FileCopyrightText: 2023 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgsystembell_v1.h"

#include "clientconnection.h"
#include "display.h"
#include "surface.h"

#include <qwayland-server-xdg-system-bell-v1.h>

namespace KWin
{

static constexpr int version = 1;

class XdgSystemBellV1InterfacePrivate : public QtWaylandServer::xdg_system_bell_v1
{
public:
    XdgSystemBellV1InterfacePrivate(XdgSystemBellV1Interface *q, Display *display);

    XdgSystemBellV1Interface *q;
    Display *display;

protected:
    void xdg_system_bell_v1_ring(Resource *resource, wl_resource *surface) override;
    void xdg_system_bell_v1_destroy(Resource *resource) override;
};

XdgSystemBellV1InterfacePrivate::XdgSystemBellV1InterfacePrivate(XdgSystemBellV1Interface *q, Display *display)
    : QtWaylandServer::xdg_system_bell_v1(*display, version)
    , q(q)
    , display(display)
{
}

void XdgSystemBellV1InterfacePrivate::xdg_system_bell_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgSystemBellV1InterfacePrivate::xdg_system_bell_v1_ring(Resource *resource, wl_resource *surface_resource)
{
    if (surface_resource) {
        auto surface = SurfaceInterface::get(surface_resource);
        Q_EMIT q->ringSurface(surface);
        return;
    }
    Q_EMIT q->ring(display->getConnection(resource->client()));
}

XdgSystemBellV1Interface::XdgSystemBellV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<XdgSystemBellV1InterfacePrivate>(this, display))
{
}

XdgSystemBellV1Interface::~XdgSystemBellV1Interface() = default;

void XdgSystemBellV1Interface::remove()
{
    d->globalRemove();
}
}
