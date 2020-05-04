/*
    SPDX-FileCopyrightText: 2020 Benjamin Port <benjamin.port@enioka.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "keyboard_shortcuts_inhibit_interface.h"

#include <qwayland-server-keyboard-shortcuts-inhibit-unstable-v1.h>

#include "display.h"
#include "seat_interface.h"
#include "surface_interface.h"

static const int s_version = 1;

namespace KWaylandServer
{
class KeyboardShortcutsInhibitorInterfacePrivate : public QtWaylandServer::zwp_keyboard_shortcuts_inhibitor_v1
{
public:
    KeyboardShortcutsInhibitorInterfacePrivate(SurfaceInterface *surface, SeatInterface *seat,
                                               KeyboardShortcutsInhibitManagerInterface *manager,
                                               KeyboardShortcutsInhibitorInterface *q, wl_resource *resource);

    KeyboardShortcutsInhibitorInterface *q;
    QPointer<KeyboardShortcutsInhibitManagerInterface> m_manager;
    SurfaceInterface *const m_surface;
    SeatInterface *const m_seat;
    bool m_active;

protected:
    void zwp_keyboard_shortcuts_inhibitor_v1_destroy_resource(Resource *resource) override;

    void zwp_keyboard_shortcuts_inhibitor_v1_destroy(Resource *resource) override;
};

class KeyboardShortcutsInhibitManagerInterfacePrivate
    : public QtWaylandServer::zwp_keyboard_shortcuts_inhibit_manager_v1
{
public:
    KeyboardShortcutsInhibitManagerInterfacePrivate(Display *display, KeyboardShortcutsInhibitManagerInterface *q);

    void zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts(Resource *resource, uint32_t id,
                                                                     struct ::wl_resource *surface_resource,
                                                                     struct ::wl_resource *seat_resource) override;

    KeyboardShortcutsInhibitorInterface *findInhibitor(SurfaceInterface *surface, SeatInterface *seat) const;

    void removeInhibitor(SurfaceInterface *const surface, SeatInterface *const seat);

    KeyboardShortcutsInhibitManagerInterface *q;
    Display *const m_display;
    QHash<QPair<SurfaceInterface *, SeatInterface *>, KeyboardShortcutsInhibitorInterface *> m_inhibitors;

protected:
    void zwp_keyboard_shortcuts_inhibit_manager_v1_destroy(Resource *resource) override;
};

KeyboardShortcutsInhibitorInterfacePrivate::KeyboardShortcutsInhibitorInterfacePrivate(SurfaceInterface *surface,
                                                                                       SeatInterface *seat,
                                                                                       KeyboardShortcutsInhibitManagerInterface *manager,
                                                                                       KeyboardShortcutsInhibitorInterface *q,
                                                                                       wl_resource *resource)
    : zwp_keyboard_shortcuts_inhibitor_v1(resource), q(q), m_manager(manager), m_surface(surface), m_seat(seat),
      m_active(false)
{
}

void KeyboardShortcutsInhibitorInterfacePrivate::zwp_keyboard_shortcuts_inhibitor_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void
KeyboardShortcutsInhibitorInterfacePrivate::zwp_keyboard_shortcuts_inhibitor_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    // Ensure manager don't track anymore this inhibitor
    if (m_manager) {
        m_manager->removeInhibitor(m_surface, m_seat);
    }
    delete q;
}

KeyboardShortcutsInhibitorInterface::KeyboardShortcutsInhibitorInterface(SurfaceInterface *surface, SeatInterface *seat,
                                                                         KeyboardShortcutsInhibitManagerInterface *manager,
                                                                         wl_resource *resource)
    : QObject(nullptr), d(new KeyboardShortcutsInhibitorInterfacePrivate(surface, seat, manager, this, resource))
{
}

KeyboardShortcutsInhibitorInterface::~KeyboardShortcutsInhibitorInterface() = default;

void KeyboardShortcutsInhibitorInterface::setActive(bool active)
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

bool KeyboardShortcutsInhibitorInterface::isActive() const
{
    return d->m_active;
}

SeatInterface *KeyboardShortcutsInhibitorInterface::seat() const
{
    return d->m_seat;
}

SurfaceInterface *KeyboardShortcutsInhibitorInterface::surface() const
{
    return d->m_surface;
}

KeyboardShortcutsInhibitManagerInterfacePrivate::KeyboardShortcutsInhibitManagerInterfacePrivate(Display *display,
                                                                                                 KeyboardShortcutsInhibitManagerInterface *q)
    : zwp_keyboard_shortcuts_inhibit_manager_v1(*display, s_version), q(q), m_display(display)
{
}

void KeyboardShortcutsInhibitManagerInterfacePrivate::zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts(
    Resource *resource, uint32_t id, wl_resource *surface_resource, wl_resource *seat_resource)
{
    SeatInterface *seat = SeatInterface::get(seat_resource);
    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);
    if (m_inhibitors.contains({surface, seat})) {
        wl_resource_post_error(resource->handle, error::error_already_inhibited,
                               "the shortcuts are already inhibited for this surface and seat");
        return;
    }

    wl_resource *inhibitorResource = wl_resource_create(resource->client(),
                                                        &zwp_keyboard_shortcuts_inhibitor_v1_interface,
                                                        resource->version(), id);
    auto inhibitor = new KeyboardShortcutsInhibitorInterface(surface, seat, q, inhibitorResource);
    m_inhibitors[{surface, seat}] = inhibitor;
    Q_EMIT q->inhibitorCreated(inhibitor);
    inhibitor->setActive(true);
}

KeyboardShortcutsInhibitorInterface *KeyboardShortcutsInhibitManagerInterfacePrivate::findInhibitor(SurfaceInterface *surface, SeatInterface *seat) const
{
    return m_inhibitors.value({surface, seat}, nullptr);
}

void KeyboardShortcutsInhibitManagerInterfacePrivate::zwp_keyboard_shortcuts_inhibit_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void KeyboardShortcutsInhibitManagerInterfacePrivate::removeInhibitor(SurfaceInterface *const surface,
                                                                      SeatInterface *const seat)
{
    m_inhibitors.remove({surface, seat});
}

KeyboardShortcutsInhibitManagerInterface::KeyboardShortcutsInhibitManagerInterface(Display *display, QObject *parent)
    : QObject(parent), d(new KeyboardShortcutsInhibitManagerInterfacePrivate(display, this))
{
}

KeyboardShortcutsInhibitManagerInterface::~KeyboardShortcutsInhibitManagerInterface() = default;

KeyboardShortcutsInhibitorInterface *KeyboardShortcutsInhibitManagerInterface::findInhibitor(SurfaceInterface *surface, SeatInterface *seat) const
{
    return d->findInhibitor(surface, seat);
}

void KeyboardShortcutsInhibitManagerInterface::removeInhibitor(SurfaceInterface *surface, SeatInterface *seat)
{
    d->removeInhibitor(surface, seat);
}

}
