/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "pointerconstraints_interface_p.h"
#include "display.h"
#include "pointer_interface.h"
#include "region_interface.h"
#include "surface_interface_p.h"

#include <wayland-pointer-constraints-unstable-v1-server-protocol.h>

namespace KWayland
{
namespace Server
{

class PointerConstraintsUnstableV1Interface::Private : public PointerConstraintsInterface::Private
{
public:
    Private(PointerConstraintsUnstableV1Interface *q, Display *d);

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    template <class T>
    void createConstraint(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface, wl_resource *pointer, wl_resource *region, uint32_t lifetime);

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void lockPointerCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * surface, wl_resource * pointer, wl_resource * region, uint32_t lifetime);
    static void confinePointerCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * surface, wl_resource * pointer, wl_resource * region, uint32_t lifetime);

    PointerConstraintsUnstableV1Interface *q;
    static const struct zwp_pointer_constraints_v1_interface s_interface;
    static const quint32 s_version;
};

class LockedPointerUnstableV1Interface::Private : public LockedPointerInterface::Private
{
public:
    Private(LockedPointerUnstableV1Interface *q, PointerConstraintsUnstableV1Interface *c, wl_resource *parentResource);
    ~Private();

    void updateLocked() override;

private:
    static void setCursorPositionHintCallback(wl_client *client, wl_resource *resource, wl_fixed_t surface_x, wl_fixed_t surface_y);
    static void setRegionCallback(wl_client *client, wl_resource *resource, wl_resource * region);

    LockedPointerUnstableV1Interface *q_func() {
        return reinterpret_cast<LockedPointerUnstableV1Interface *>(q);
    }

    static const struct zwp_locked_pointer_v1_interface s_interface;
};

const quint32 PointerConstraintsUnstableV1Interface::Private::s_version = 1;

#ifndef K_DOXYGEN
const struct zwp_pointer_constraints_v1_interface PointerConstraintsUnstableV1Interface::Private::s_interface = {
    destroyCallback,
    lockPointerCallback,
    confinePointerCallback
};
#endif

void PointerConstraintsUnstableV1Interface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    wl_resource_destroy(resource);
}

template <class T>
void PointerConstraintsUnstableV1Interface::Private::createConstraint(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface, wl_resource *pointer, wl_resource *region, uint32_t lifetime)
{
    auto s = SurfaceInterface::get(surface);
    auto p = PointerInterface::get(pointer);
    if (!s || !p) {
        // send error?
        return;
    }
    if (!s->lockedPointer().isNull() || !s->confinedPointer().isNull()) {
        wl_resource_post_error(s->resource(), ZWP_POINTER_CONSTRAINTS_V1_ERROR_ALREADY_CONSTRAINED, "Surface already constrained");
        return;
    }
    auto constraint = new T(q, resource);
    switch (lifetime) {
    case ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT:
        constraint->d_func()->lifeTime = T::LifeTime::Persistent;
        break;
    case ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT: // fall through
    default:
        constraint->d_func()->lifeTime = T::LifeTime::OneShot;
        break;
    }
    auto r = RegionInterface::get(region);
    constraint->d_func()->region = r ? r->region() : QRegion();
    constraint->d_func()->create(display->getConnection(client), version, id);
    s->d_func()->installPointerConstraint(constraint);
}

void PointerConstraintsUnstableV1Interface::Private::lockPointerCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface, wl_resource *pointer, wl_resource *region, uint32_t lifetime)
{
    cast(resource)->createConstraint<LockedPointerUnstableV1Interface>(client, resource, id, surface, pointer, region, lifetime);
}

void PointerConstraintsUnstableV1Interface::Private::confinePointerCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface, wl_resource *pointer, wl_resource *region, uint32_t lifetime)
{
    cast(resource)->createConstraint<ConfinedPointerUnstableV1Interface>(client, resource, id, surface, pointer, region, lifetime);
}

PointerConstraintsUnstableV1Interface::Private::Private(PointerConstraintsUnstableV1Interface *q, Display *d)
    : PointerConstraintsInterface::Private(PointerConstraintsInterfaceVersion::UnstableV1, q, d, &zwp_pointer_constraints_v1_interface, s_version)
    , q(q)
{
}

void PointerConstraintsUnstableV1Interface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&zwp_pointer_constraints_v1_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    // TODO: should we track?
}

void PointerConstraintsUnstableV1Interface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
    // TODO: implement?
}

PointerConstraintsUnstableV1Interface::PointerConstraintsUnstableV1Interface(Display *display, QObject *parent)
    : PointerConstraintsInterface(new Private(this, display), parent)
{
}

PointerConstraintsUnstableV1Interface::~PointerConstraintsUnstableV1Interface() = default;

#ifndef K_DOXYGEN
const struct zwp_locked_pointer_v1_interface LockedPointerUnstableV1Interface::Private::s_interface = {
    resourceDestroyedCallback,
    setCursorPositionHintCallback,
    setRegionCallback
};
#endif

void LockedPointerUnstableV1Interface::Private::setCursorPositionHintCallback(wl_client *client, wl_resource *resource, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    Q_UNUSED(client)
    auto p = cast<Private>(resource);
    p->pendingHint = QPointF(wl_fixed_to_double(surface_x), wl_fixed_to_double(surface_y));
    p->hintIsSet = true;
}

void LockedPointerUnstableV1Interface::Private::setRegionCallback(wl_client *client, wl_resource *resource, wl_resource * region)
{
    Q_UNUSED(client)
    auto p = cast<Private>(resource);
    auto r = RegionInterface::get(region);
    p->pendingRegion = r ? r->region() : QRegion();
    p->regionIsSet = true;
}

void LockedPointerUnstableV1Interface::Private::updateLocked()
{
    if (!resource) {
        return;
    }
    if (locked) {
        zwp_locked_pointer_v1_send_locked(resource);
    } else {
        zwp_locked_pointer_v1_send_unlocked(resource);
    }
}

LockedPointerUnstableV1Interface::Private::Private(LockedPointerUnstableV1Interface *q, PointerConstraintsUnstableV1Interface *c, wl_resource *parentResource)
    : LockedPointerInterface::Private(PointerConstraintsInterfaceVersion::UnstableV1, q, c, parentResource, &zwp_locked_pointer_v1_interface, &s_interface)
{
}

LockedPointerUnstableV1Interface::LockedPointerUnstableV1Interface(PointerConstraintsUnstableV1Interface *parent, wl_resource *parentResource)
    : LockedPointerInterface(new Private(this, parent, parentResource))
{
}

LockedPointerUnstableV1Interface::Private::~Private() = default;

LockedPointerUnstableV1Interface::~LockedPointerUnstableV1Interface() = default;

LockedPointerUnstableV1Interface::Private *LockedPointerUnstableV1Interface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

class ConfinedPointerUnstableV1Interface::Private : public ConfinedPointerInterface::Private
{
public:
    Private(ConfinedPointerUnstableV1Interface *q, PointerConstraintsUnstableV1Interface *c, wl_resource *parentResource);
    ~Private();

    void updateConfined() override;

private:
    static void setRegionCallback(wl_client *client, wl_resource *resource, wl_resource * region);

    ConfinedPointerUnstableV1Interface *q_func() {
        return reinterpret_cast<ConfinedPointerUnstableV1Interface *>(q);
    }

    static const struct zwp_confined_pointer_v1_interface s_interface;
};

#ifndef K_DOXYGEN
const struct zwp_confined_pointer_v1_interface ConfinedPointerUnstableV1Interface::Private::s_interface = {
    resourceDestroyedCallback,
    setRegionCallback
};
#endif

void ConfinedPointerUnstableV1Interface::Private::setRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region)
{
    Q_UNUSED(client)
    auto p = cast<Private>(resource);
    auto r = RegionInterface::get(region);
    p->pendingRegion = r ? r->region() : QRegion();
    p->regionIsSet = true;
}

ConfinedPointerUnstableV1Interface::Private::Private(ConfinedPointerUnstableV1Interface *q, PointerConstraintsUnstableV1Interface *c, wl_resource *parentResource)
    : ConfinedPointerInterface::Private(PointerConstraintsInterfaceVersion::UnstableV1, q, c, parentResource, &zwp_confined_pointer_v1_interface, &s_interface)
{
}

ConfinedPointerUnstableV1Interface::ConfinedPointerUnstableV1Interface(PointerConstraintsUnstableV1Interface *parent, wl_resource *parentResource)
    : ConfinedPointerInterface(new Private(this, parent, parentResource))
{
}

ConfinedPointerUnstableV1Interface::Private::~Private() = default;

ConfinedPointerUnstableV1Interface::~ConfinedPointerUnstableV1Interface() = default;

void ConfinedPointerUnstableV1Interface::Private::updateConfined()
{
    if (!resource) {
        return;
    }
    if (confined) {
        zwp_confined_pointer_v1_send_confined(resource);
    } else {
        zwp_confined_pointer_v1_send_unconfined(resource);
    }
}

ConfinedPointerUnstableV1Interface::Private *ConfinedPointerUnstableV1Interface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}
