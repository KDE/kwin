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

static QRegion mapScaleOverride(const QRegion &region, qreal scaleOverride)
{
    QRegion out;
    for (const QRect &rect : region) {
        out += QRect(rect.topLeft() / scaleOverride, rect.size() / scaleOverride);
    }
    return out;
}

PointerConstraintsV1InterfacePrivate::PointerConstraintsV1InterfacePrivate(Display *display)
    : QtWaylandServer::zwp_pointer_constraints_v1(*display, s_version)
{
}

static QRegion regionFromResource(::wl_resource *resource)
{
    const RegionInterface *region = RegionInterface::get(resource);
    return region ? region->region() : QRegion();
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

    new LockedPointerV1Interface(surface, LockedPointerV1Interface::LifeTime(lifetime), regionFromResource(region_resource), lockedPointerResource);
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

    new ConfinedPointerV1Interface(surface, ConfinedPointerV1Interface::LifeTime(lifetime), regionFromResource(region_resource), confinedPointerResource);
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
                                                                 LockedPointerV1Interface::LifeTime lifeTime,
                                                                 const QRegion &region,
                                                                 ::wl_resource *resource)
    : QtWaylandServer::zwp_locked_pointer_v1(resource)
    , SurfaceExtension(surface)
    , q(q)
    , surface(surface)
    , lifeTime(lifeTime)
{
    pending.region = region;

    apply(&pending);

    pending = LockedPointerV1Commit{};
}

void LockedPointerV1InterfacePrivate::apply(LockedPointerV1Commit *commit)
{
    const QRegion oldRegion = effectiveRegion;
    const QPointF oldHint = hint;

    if (commit->region.has_value()) {
        region = mapScaleOverride(commit->region.value(), surface->scaleOverride());
    }
    if (commit->hint.has_value()) {
        hint = commit->hint.value() / surface->scaleOverride();
    }

    effectiveRegion = surface->input();
    if (!region.isEmpty()) {
        effectiveRegion &= region;
    }

    if (oldRegion != effectiveRegion) {
        Q_EMIT q->regionChanged();
    }
    if (oldHint != hint) {
        Q_EMIT q->cursorPositionHintChanged();
    }
}

void LockedPointerV1InterfacePrivate::zwp_locked_pointer_v1_destroy_resource(Resource *resource)
{
    Q_EMIT q->aboutToBeDestroyed();
    delete q;
}

void LockedPointerV1InterfacePrivate::zwp_locked_pointer_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void LockedPointerV1InterfacePrivate::zwp_locked_pointer_v1_set_cursor_position_hint(Resource *resource, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    pending.hint = QPointF(wl_fixed_to_double(surface_x), wl_fixed_to_double(surface_y));
}

void LockedPointerV1InterfacePrivate::zwp_locked_pointer_v1_set_region(Resource *resource, ::wl_resource *region_resource)
{
    pending.region = regionFromResource(region_resource);
}

LockedPointerV1Interface::LockedPointerV1Interface(SurfaceInterface *surface,
                                                   LifeTime lifeTime,
                                                   const QRegion &region,
                                                   ::wl_resource *resource)
    : d(new LockedPointerV1InterfacePrivate(this, surface, lifeTime, region, resource))
{
    SurfaceInterfacePrivate::get(surface)->installPointerConstraint(this);
}

LockedPointerV1Interface::~LockedPointerV1Interface()
{
}

LockedPointerV1Interface::LifeTime LockedPointerV1Interface::lifeTime() const
{
    return d->lifeTime;
}

QRegion LockedPointerV1Interface::region() const
{
    return d->effectiveRegion;
}

QPointF LockedPointerV1Interface::cursorPositionHint() const
{
    return d->hint;
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
        d->hint = QPointF(-1, -1);
    }
    d->isLocked = locked;
    if (d->isLocked) {
        d->send_locked();
    } else {
        d->send_unlocked();
    }
    Q_EMIT lockedChanged();
}

ConfinedPointerV1InterfacePrivate *ConfinedPointerV1InterfacePrivate::get(ConfinedPointerV1Interface *q)
{
    return q->d.get();
}

ConfinedPointerV1InterfacePrivate::ConfinedPointerV1InterfacePrivate(ConfinedPointerV1Interface *q,
                                                                     SurfaceInterface *surface,
                                                                     ConfinedPointerV1Interface::LifeTime lifeTime,
                                                                     const QRegion &region,
                                                                     ::wl_resource *resource)
    : QtWaylandServer::zwp_confined_pointer_v1(resource)
    , SurfaceExtension(surface)
    , q(q)
    , surface(surface)
    , lifeTime(lifeTime)
{
    pending.region = region;

    apply(&pending);

    pending = ConfinedPointerV1Commit{};
}

void ConfinedPointerV1InterfacePrivate::apply(ConfinedPointerV1Commit *commit)
{
    const QRegion oldRegion = effectiveRegion;

    if (commit->region.has_value()) {
        region = mapScaleOverride(commit->region.value(), surface->scaleOverride());
    }

    effectiveRegion = surface->input();
    if (!region.isEmpty()) {
        effectiveRegion &= region;
    }

    if (oldRegion != effectiveRegion) {
        Q_EMIT q->regionChanged();
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
    pending.region = regionFromResource(region_resource);
}

ConfinedPointerV1Interface::ConfinedPointerV1Interface(SurfaceInterface *surface,
                                                       LifeTime lifeTime,
                                                       const QRegion &region,
                                                       ::wl_resource *resource)
    : d(new ConfinedPointerV1InterfacePrivate(this, surface, lifeTime, region, resource))
{
    SurfaceInterfacePrivate::get(surface)->installPointerConstraint(this);
}

ConfinedPointerV1Interface::~ConfinedPointerV1Interface()
{
}

ConfinedPointerV1Interface::LifeTime ConfinedPointerV1Interface::lifeTime() const
{
    return d->lifeTime;
}

QRegion ConfinedPointerV1Interface::region() const
{
    return d->effectiveRegion;
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
}

} // namespace KWin

#include "moc_pointerconstraints_v1.cpp"
