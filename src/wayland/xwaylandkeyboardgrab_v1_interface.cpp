/*
    SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xwaylandkeyboardgrab_v1_interface.h"

#include <qwayland-server-xwayland-keyboard-grab-unstable-v1.h>

#include "display.h"
#include "seat_interface.h"
#include "surface_interface.h"

constexpr int version = 1;

namespace KWaylandServer
{

class XWaylandKeyboardGrabManagerV1InterfacePrivate : QtWaylandServer::zwp_xwayland_keyboard_grab_manager_v1
{
public:
    XWaylandKeyboardGrabManagerV1InterfacePrivate(Display *display, XWaylandKeyboardGrabManagerV1Interface *q);
    XWaylandKeyboardGrabManagerV1Interface *const q;
    QHash<QPair<const SurfaceInterface *, const SeatInterface *>, XWaylandKeyboardGrabV1Interface *> m_grabs;

protected:
    void zwp_xwayland_keyboard_grab_manager_v1_destroy(Resource *resource) override;
    void zwp_xwayland_keyboard_grab_manager_v1_grab_keyboard(Resource *resource, uint32_t id, wl_resource *surface, wl_resource *seat) override;
};

class XWaylandKeyboardGrabV1InterfacePrivate : QtWaylandServer::zwp_xwayland_keyboard_grab_v1
{
public:
    XWaylandKeyboardGrabV1InterfacePrivate(XWaylandKeyboardGrabV1Interface *q, wl_resource *resource);
    XWaylandKeyboardGrabV1Interface *const q;

protected:
    void zwp_xwayland_keyboard_grab_v1_destroy(Resource *resource) override;
    void zwp_xwayland_keyboard_grab_v1_destroy_resource(Resource *resource) override;
};

XWaylandKeyboardGrabManagerV1InterfacePrivate::XWaylandKeyboardGrabManagerV1InterfacePrivate(Display *display, XWaylandKeyboardGrabManagerV1Interface *q)
    : zwp_xwayland_keyboard_grab_manager_v1(*display, version)
    , q(q)
{
}

void XWaylandKeyboardGrabManagerV1InterfacePrivate::zwp_xwayland_keyboard_grab_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XWaylandKeyboardGrabManagerV1InterfacePrivate::zwp_xwayland_keyboard_grab_manager_v1_grab_keyboard(Resource *resource, uint32_t id, wl_resource *surface, wl_resource *seat)
{
    wl_resource *keyboardGrab = wl_resource_create(resource->client(), &zwp_xwayland_keyboard_grab_v1_interface, version, id);
    if (!keyboardGrab) {
        wl_client_post_no_memory(resource->client());
        return;
    }
    const auto surfaceInterface = SurfaceInterface::get(surface);
    const auto seatInterface = SeatInterface::get(seat);
    auto grab = new XWaylandKeyboardGrabV1Interface(keyboardGrab);
    QObject::connect(grab, &QObject::destroyed, q, [this, surfaceInterface, seatInterface] {
        m_grabs.remove({surfaceInterface, seatInterface});
    });
    m_grabs.insert({SurfaceInterface::get(surface), SeatInterface::get(seat)}, grab);
}

XWaylandKeyboardGrabManagerV1Interface::XWaylandKeyboardGrabManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<XWaylandKeyboardGrabManagerV1InterfacePrivate>(display, this))
{
}

XWaylandKeyboardGrabManagerV1Interface::~XWaylandKeyboardGrabManagerV1Interface()
{
}

bool XWaylandKeyboardGrabManagerV1Interface::hasGrab(SurfaceInterface *surface, SeatInterface *seat) const
{
    return d->m_grabs.contains({surface, seat});
}

XWaylandKeyboardGrabV1InterfacePrivate::XWaylandKeyboardGrabV1InterfacePrivate(XWaylandKeyboardGrabV1Interface *q, wl_resource *resource)
    : zwp_xwayland_keyboard_grab_v1(resource)
    , q(q)
{
}

void XWaylandKeyboardGrabV1InterfacePrivate::zwp_xwayland_keyboard_grab_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XWaylandKeyboardGrabV1InterfacePrivate::zwp_xwayland_keyboard_grab_v1_destroy_resource(Resource *resource)
{
    delete q;
}

XWaylandKeyboardGrabV1Interface::XWaylandKeyboardGrabV1Interface(wl_resource *resource)
    : d(std::make_unique<XWaylandKeyboardGrabV1InterfacePrivate>(this, resource))
{
}

XWaylandKeyboardGrabV1Interface::~XWaylandKeyboardGrabV1Interface()
{
}

}
