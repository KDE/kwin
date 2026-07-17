/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "pointerconstraints_v1.h"
#include "display.h"
#include "pointer.h"
#include "pointerconstraints_v1_p.h"
#include "region_p.h"
#include "surface_p.h"

namespace KWin
{

static const int s_version = 1;

PointerConstraintsV1InterfacePrivate::PointerConstraintsV1InterfacePrivate(Display *display)
    : QtWaylandServer::zwp_pointer_constraints_v1(*display, s_version)
{
}

static Region regionFromResource(::wl_resource *resource)
{
    const RegionInterface *region = RegionInterface::get(resource);
    return region ? region->region() : Region();
}

void PointerConstraintsV1InterfacePrivate::zwp_pointer_constraints_v1_lock_pointer(Resource *resource,
                                                                                   uint32_t id,
                                                                                   ::wl_resource *surface_resource,
                                                                                   ::wl_resource *pointer_resource,
                                                                                   ::wl_resource *region_resource,
                                                                                   uint32_t lifetime)
{
    PointerInterface *pointer = PointerInterface::get(pointer_resource);
    if (!pointer) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT, "invalid pointer");
        return;
    }

    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);
    if (!surface) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT, "invalid surface");
        return;
    }

    if (surface->lockedPointer() || surface->confinedPointer()) {
        wl_resource_post_error(resource->handle, error_already_constrained, "the surface is already constrained");
        return;
    }

    if (lifetime != lifetime_oneshot && lifetime != lifetime_persistent) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT, "unknown lifetime %d", lifetime);
        return;
    }

    wl_resource *lockedPointerResource = wl_resource_create(resource->client(), &zwp_locked_pointer_v1_interface, resource->version(), id);
    if (!lockedPointerResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    RegionF region = regionFromResource(region_resource).scaled(1.0 / surface->clientToCompositorScale() / surface->serverScale());
    new LockedPointerV1Interface(surface, PointerConstraintLifetime(lifetime), std::move(region), lockedPointerResource);
}

void PointerConstraintsV1InterfacePrivate::zwp_pointer_constraints_v1_confine_pointer(Resource *resource,
                                                                                      uint32_t id,
                                                                                      ::wl_resource *surface_resource,
                                                                                      ::wl_resource *pointer_resource,
                                                                                      ::wl_resource *region_resource,
                                                                                      uint32_t lifetime)
{
    PointerInterface *pointer = PointerInterface::get(pointer_resource);
    if (!pointer) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT, "invalid pointer");
        return;
    }

    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);
    if (!surface) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT, "invalid surface");
        return;
    }

    if (lifetime != lifetime_oneshot && lifetime != lifetime_persistent) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT, "unknown lifetime %d", lifetime);
        return;
    }

    if (surface->lockedPointer() || surface->confinedPointer()) {
        wl_resource_post_error(resource->handle, error_already_constrained, "the surface is already constrained");
        return;
    }

    wl_resource *confinedPointerResource = wl_resource_create(resource->client(), &zwp_confined_pointer_v1_interface, resource->version(), id);
    if (!confinedPointerResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    RegionF region = regionFromResource(region_resource).scaled(1.0 / surface->clientToCompositorScale() / surface->serverScale());
    new ConfinedPointerV1Interface(surface, PointerConstraintLifetime(lifetime), std::move(region), confinedPointerResource);
}

void PointerConstraintsV1InterfacePrivate::zwp_pointer_constraints_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

PointerConstraintsV1Interface::PointerConstraintsV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new PointerConstraintsV1InterfacePrivate(display))
{
}

PointerConstraintsV1Interface::~PointerConstraintsV1Interface()
{
}

LockedPointerV1InterfacePrivate *LockedPointerV1InterfacePrivate::get(LockedPointerV1Interface *q)
{
    return q->d.get();
}

LockedPointerV1InterfacePrivate::LockedPointerV1InterfacePrivate(LockedPointerV1Interface *q,
                                                                 SurfaceInterface *surface,
                                                                 PointerConstraintLifetime lifeTime,
                                                                 const RegionF &region,
                                                                 ::wl_resource *resource)
    : QtWaylandServer::zwp_locked_pointer_v1(resource)
    , q(q)
    , lifeTime(lifeTime)
    , m_surface(surface)
{
    auto priv = SurfaceInterfacePrivate::get(surface);
    priv->pending->pointerLockHint.reset();
    priv->pending->pointerLockRegion = region;
    priv->pending->confinementLifetime = lifeTime;
    priv->pending->committed |= SurfaceState::Field::PointerLockHint | SurfaceState::Field::PointerLockRegion;
    priv->lockedPointer = q;
}

LockedPointerV1InterfacePrivate::~LockedPointerV1InterfacePrivate()
{
    if (m_surface) {
        // NOTE the position hint is intentionally not reset,
        // since it will be used when the next commit is applied.
        auto priv = SurfaceInterfacePrivate::get(m_surface);
        priv->pending->pointerLockRegion.reset();
        priv->pending->committed |= SurfaceState::Field::PointerLockRegion;
        priv->lockedPointer = nullptr;
    }
}

void LockedPointerV1InterfacePrivate::zwp_locked_pointer_v1_destroy_resource(Resource *resource)
{
    delete q;
}

void LockedPointerV1InterfacePrivate::zwp_locked_pointer_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void LockedPointerV1InterfacePrivate::zwp_locked_pointer_v1_set_cursor_position_hint(Resource *resource, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    if (Q_UNLIKELY(!m_surface)) {
        return;
    }
    auto priv = SurfaceInterfacePrivate::get(m_surface);
    priv->pending->pointerLockHint = QPointF(wl_fixed_to_double(surface_x), wl_fixed_to_double(surface_y)) / m_surface->clientToCompositorScale() / m_surface->serverScale();
    priv->pending->committed |= SurfaceState::Field::PointerLockHint;
}

void LockedPointerV1InterfacePrivate::zwp_locked_pointer_v1_set_region(Resource *resource, ::wl_resource *region_resource)
{
    if (Q_UNLIKELY(!m_surface)) {
        return;
    }
    auto priv = SurfaceInterfacePrivate::get(m_surface);
    priv->pending->pointerLockRegion = regionFromResource(region_resource).scaled(1.0 / m_surface->clientToCompositorScale() / m_surface->serverScale());
    priv->pending->committed |= SurfaceState::Field::PointerLockRegion;
}

LockedPointerV1Interface::LockedPointerV1Interface(SurfaceInterface *surface,
                                                   PointerConstraintLifetime lifeTime,
                                                   const RegionF &region,
                                                   ::wl_resource *resource)
    : d(new LockedPointerV1InterfacePrivate(this, surface, lifeTime, region, resource))
{
}

LockedPointerV1Interface::~LockedPointerV1Interface()
{
}

PointerConstraintLifetime LockedPointerV1Interface::lifeTime() const
{
    return d->lifeTime;
}

bool LockedPointerV1Interface::isLocked() const
{
    return d->isLocked;
}

void LockedPointerV1Interface::setLocked(bool locked)
{
    if (d->isLocked == locked) {
        return;
    }
    if (!locked) {
        SurfaceInterfacePrivate::get(d->m_surface)->pending->pointerLockHint.reset();
    }
    d->isLocked = locked;
    if (d->isLocked) {
        d->send_locked();
    } else {
        d->send_unlocked();
    }
    Q_EMIT lockedChanged();
    if (!locked && d->lifeTime == PointerConstraintLifetime::OneShot) {
        delete this;
    }
}

ConfinedPointerV1InterfacePrivate *ConfinedPointerV1InterfacePrivate::get(ConfinedPointerV1Interface *q)
{
    return q->d.get();
}

ConfinedPointerV1InterfacePrivate::ConfinedPointerV1InterfacePrivate(ConfinedPointerV1Interface *q,
                                                                     SurfaceInterface *surface,
                                                                     PointerConstraintLifetime lifeTime,
                                                                     const RegionF &region,
                                                                     ::wl_resource *resource)
    : QtWaylandServer::zwp_confined_pointer_v1(resource)
    , q(q)
    , lifeTime(lifeTime)
    , m_surface(surface)
{
    auto priv = SurfaceInterfacePrivate::get(surface);
    priv->pending->pointerConfinementRegion = region;
    priv->pending->confinementLifetime = lifeTime;
    priv->pending->committed |= SurfaceState::Field::PointerConfinementRegion;
    priv->confinedPointer = q;
}

ConfinedPointerV1InterfacePrivate::~ConfinedPointerV1InterfacePrivate()
{
    if (m_surface) {
        auto priv = SurfaceInterfacePrivate::get(m_surface);
        priv->pending->pointerConfinementRegion.reset();
        priv->pending->committed |= SurfaceState::Field::PointerConfinementRegion;
        priv->confinedPointer = nullptr;
    }
}

void ConfinedPointerV1InterfacePrivate::zwp_confined_pointer_v1_destroy_resource(Resource *resource)
{
    delete q;
}

void ConfinedPointerV1InterfacePrivate::zwp_confined_pointer_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ConfinedPointerV1InterfacePrivate::zwp_confined_pointer_v1_set_region(Resource *resource, ::wl_resource *region_resource)
{
    if (Q_UNLIKELY(!m_surface)) {
        return;
    }
    auto priv = SurfaceInterfacePrivate::get(m_surface);
    priv->pending->pointerConfinementRegion = regionFromResource(region_resource).scaled(1.0 / m_surface->clientToCompositorScale() / m_surface->serverScale());
    priv->pending->committed |= SurfaceState::Field::PointerConfinementRegion;
}

ConfinedPointerV1Interface::ConfinedPointerV1Interface(SurfaceInterface *surface,
                                                       PointerConstraintLifetime lifeTime,
                                                       const RegionF &region,
                                                       ::wl_resource *resource)
    : d(new ConfinedPointerV1InterfacePrivate(this, surface, lifeTime, region, resource))
{
}

ConfinedPointerV1Interface::~ConfinedPointerV1Interface()
{
}

PointerConstraintLifetime ConfinedPointerV1Interface::lifeTime() const
{
    return d->lifeTime;
}

bool ConfinedPointerV1Interface::isConfined() const
{
    return d->isConfined;
}

void ConfinedPointerV1Interface::setConfined(bool confined)
{
    if (d->isConfined == confined) {
        return;
    }
    d->isConfined = confined;
    if (d->isConfined) {
        d->send_confined();
    } else {
        d->send_unconfined();
    }
    Q_EMIT confinedChanged();
    if (!confined && d->lifeTime == PointerConstraintLifetime::OneShot) {
        delete this;
    }
}

} // namespace KWin

#include "moc_pointerconstraints_v1.cpp"
