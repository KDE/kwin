/*
    SPDX-FileCopyrightText: 2020 Benjamin Port <benjamin.port@enioka.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "keyboard_shortcuts_inhibit_v1_interface.h"

#include <qwayland-server-keyboard-shortcuts-inhibit-unstable-v1.h>

#include "display.h"
#include "seat_interface.h"
#include "surface_interface.h"

static const int s_version = 1;

namespace KWaylandServer
{
class KeyboardShortcutsInhibitorV1InterfacePrivate : public QtWaylandServer::zwp_keyboard_shortcuts_inhibitor_v1
{
public:
    KeyboardShortcutsInhibitorV1InterfacePrivate(SurfaceInterface *surface,
                                                 SeatInterface *seat,
                                                 KeyboardShortcutsInhibitManagerV1Interface *manager,
                                                 KeyboardShortcutsInhibitorV1Interface *q,
                                                 wl_resource *resource);

    KeyboardShortcutsInhibitorV1Interface *q;
    QPointer<KeyboardShortcutsInhibitManagerV1Interface> m_manager;
    SurfaceInterface *const m_surface;
    SeatInterface *const m_seat;
    bool m_active;

protected:
    void zwp_keyboard_shortcuts_inhibitor_v1_destroy_resource(Resource *resource) override;

    void zwp_keyboard_shortcuts_inhibitor_v1_destroy(Resource *resource) override;
};

class KeyboardShortcutsInhibitManagerV1InterfacePrivate : public QtWaylandServer::zwp_keyboard_shortcuts_inhibit_manager_v1
{
public:
    KeyboardShortcutsInhibitManagerV1InterfacePrivate(Display *display, KeyboardShortcutsInhibitManagerV1Interface *q);

    void zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts(Resource *resource,
                                                                     uint32_t id,
                                                                     struct ::wl_resource *surface_resource,
                                                                     struct ::wl_resource *seat_resource) override;

    KeyboardShortcutsInhibitorV1Interface *findInhibitor(SurfaceInterface *surface, SeatInterface *seat) const;

    void removeInhibitor(SurfaceInterface *const surface, SeatInterface *const seat);

    KeyboardShortcutsInhibitManagerV1Interface *q;
    Display *const m_display;
    QHash<QPair<SurfaceInterface *, SeatInterface *>, KeyboardShortcutsInhibitorV1Interface *> m_inhibitors;

protected:
    void zwp_keyboard_shortcuts_inhibit_manager_v1_destroy(Resource *resource) override;
};

KeyboardShortcutsInhibitorV1InterfacePrivate::KeyboardShortcutsInhibitorV1InterfacePrivate(SurfaceInterface *surface,
                                                                                           SeatInterface *seat,
                                                                                           KeyboardShortcutsInhibitManagerV1Interface *manager,
                                                                                           KeyboardShortcutsInhibitorV1Interface *q,
                                                                                           wl_resource *resource)
    : zwp_keyboard_shortcuts_inhibitor_v1(resource)
    , q(q)
    , m_manager(manager)
    , m_surface(surface)
    , m_seat(seat)
    , m_active(false)
{
}

void KeyboardShortcutsInhibitorV1InterfacePrivate::zwp_keyboard_shortcuts_inhibitor_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void KeyboardShortcutsInhibitorV1InterfacePrivate::zwp_keyboard_shortcuts_inhibitor_v1_destroy_resource(Resource *resource)
{
    // Ensure manager don't track anymore this inhibitor
    if (m_manager) {
        m_manager->removeInhibitor(m_surface, m_seat);
    }
    delete q;
}

KeyboardShortcutsInhibitorV1Interface::KeyboardShortcutsInhibitorV1Interface(SurfaceInterface *surface,
                                                                             SeatInterface *seat,
                                                                             KeyboardShortcutsInhibitManagerV1Interface *manager,
                                                                             wl_resource *resource)
    : QObject(nullptr)
    , d(new KeyboardShortcutsInhibitorV1InterfacePrivate(surface, seat, manager, this, resource))
{
}

KeyboardShortcutsInhibitorV1Interface::~KeyboardShortcutsInhibitorV1Interface() = default;

void KeyboardShortcutsInhibitorV1Interface::setActive(bool active)
{
    if (d->m_active == active) {
        return;
    }
    d->m_active = active;
    if (active) {
        d->send_active();
    } else {
        d->send_inactive();
    }
}

bool KeyboardShortcutsInhibitorV1Interface::isActive() const
{
    return d->m_active;
}

SeatInterface *KeyboardShortcutsInhibitorV1Interface::seat() const
{
    return d->m_seat;
}

SurfaceInterface *KeyboardShortcutsInhibitorV1Interface::surface() const
{
    return d->m_surface;
}

KeyboardShortcutsInhibitManagerV1InterfacePrivate::KeyboardShortcutsInhibitManagerV1InterfacePrivate(Display *display,
                                                                                                     KeyboardShortcutsInhibitManagerV1Interface *q)
    : zwp_keyboard_shortcuts_inhibit_manager_v1(*display, s_version)
    , q(q)
    , m_display(display)
{
}

void KeyboardShortcutsInhibitManagerV1InterfacePrivate::zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts(Resource *resource,
                                                                                                                    uint32_t id,
                                                                                                                    wl_resource *surface_resource,
                                                                                                                    wl_resource *seat_resource)
{
    SeatInterface *seat = SeatInterface::get(seat_resource);
    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);
    if (m_inhibitors.contains({surface, seat})) {
        wl_resource_post_error(resource->handle, error::error_already_inhibited, "the shortcuts are already inhibited for this surface and seat");
        return;
    }

    wl_resource *inhibitorResource = wl_resource_create(resource->client(), &zwp_keyboard_shortcuts_inhibitor_v1_interface, resource->version(), id);
    auto inhibitor = new KeyboardShortcutsInhibitorV1Interface(surface, seat, q, inhibitorResource);
    m_inhibitors[{surface, seat}] = inhibitor;
    Q_EMIT q->inhibitorCreated(inhibitor);
    inhibitor->setActive(true);
}

KeyboardShortcutsInhibitorV1Interface *KeyboardShortcutsInhibitManagerV1InterfacePrivate::findInhibitor(SurfaceInterface *surface, SeatInterface *seat) const
{
    return m_inhibitors.value({surface, seat}, nullptr);
}

void KeyboardShortcutsInhibitManagerV1InterfacePrivate::zwp_keyboard_shortcuts_inhibit_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void KeyboardShortcutsInhibitManagerV1InterfacePrivate::removeInhibitor(SurfaceInterface *const surface, SeatInterface *const seat)
{
    m_inhibitors.remove({surface, seat});
}

KeyboardShortcutsInhibitManagerV1Interface::KeyboardShortcutsInhibitManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new KeyboardShortcutsInhibitManagerV1InterfacePrivate(display, this))
{
}

KeyboardShortcutsInhibitManagerV1Interface::~KeyboardShortcutsInhibitManagerV1Interface() = default;

KeyboardShortcutsInhibitorV1Interface *KeyboardShortcutsInhibitManagerV1Interface::findInhibitor(SurfaceInterface *surface, SeatInterface *seat) const
{
    return d->findInhibitor(surface, seat);
}

void KeyboardShortcutsInhibitManagerV1Interface::removeInhibitor(SurfaceInterface *surface, SeatInterface *seat)
{
    d->removeInhibitor(surface, seat);
}

}
