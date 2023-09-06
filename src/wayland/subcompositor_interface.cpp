/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "subcompositor_interface.h"
#include "display.h"
#include "subsurface_interface_p.h"
#include "surface_interface_p.h"

namespace KWaylandServer
{
static const int s_version = 1;

SubCompositorInterfacePrivate::SubCompositorInterfacePrivate(Display *display, SubCompositorInterface *q)
    : QtWaylandServer::wl_subcompositor(*display, s_version)
    , q(q)
{
}

void SubCompositorInterfacePrivate::subcompositor_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void SubCompositorInterfacePrivate::subcompositor_get_subsurface(Resource *resource,
                                                                 uint32_t id,
                                                                 ::wl_resource *surface_resource,
                                                                 ::wl_resource *parent_resource)
{
    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);
    SurfaceInterface *parent = SurfaceInterface::get(parent_resource);

    if (!surface) {
        wl_resource_post_error(resource->handle, error_bad_surface, "no surface");
        return;
    }
    if (!parent) {
        wl_resource_post_error(resource->handle, error_bad_surface, "no parent");
        return;
    }

    if (const SurfaceRole *role = surface->role()) {
        if (role != SubSurfaceInterface::role()) {
            wl_resource_post_error(resource->handle, error_bad_surface, "the surface already has a role assigned %s", role->name().constData());
            return;
        }
    } else {
        surface->setRole(SubSurfaceInterface::role());
    }

    if (surface == parent) {
        wl_resource_post_error(resource->handle, error_bad_surface, "wl_surface@%d cannot be its own parent", wl_resource_get_id(surface_resource));
        return;
    }
    if (parent->subSurface() && parent->subSurface()->mainSurface() == surface) {
        wl_resource_post_error(resource->handle, error_bad_surface, "wl_surface@%d is an ancestor of parent", wl_resource_get_id(surface_resource));
        return;
    }

    wl_resource *subsurfaceResource = wl_resource_create(resource->client(), &wl_subsurface_interface, resource->version(), id);
    if (!subsurfaceResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    Q_EMIT q->subSurfaceCreated(new SubSurfaceInterface(surface, parent, subsurfaceResource));
}

SubCompositorInterface::SubCompositorInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new SubCompositorInterfacePrivate(display, this))

{
}

SubCompositorInterface::~SubCompositorInterface()
{
}

SubSurfaceInterfacePrivate *SubSurfaceInterfacePrivate::get(SubSurfaceInterface *subsurface)
{
    return subsurface->d.get();
}

SubSurfaceInterfacePrivate::SubSurfaceInterfacePrivate(SubSurfaceInterface *q, SurfaceInterface *surface, SurfaceInterface *parent, ::wl_resource *resource)
    : QtWaylandServer::wl_subsurface(resource)
    , q(q)
    , surface(surface)
    , parent(parent)
{
}

void SubSurfaceInterfacePrivate::subsurface_destroy_resource(Resource *resource)
{
    delete q;
}

void SubSurfaceInterfacePrivate::subsurface_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void SubSurfaceInterfacePrivate::subsurface_set_position(Resource *resource, int32_t x, int32_t y)
{
    if (!parent) {
        wl_resource_post_error(resource->handle, error_bad_surface, "no parent");
        return;
    }

    SurfaceInterfacePrivate *parentPrivate = SurfaceInterfacePrivate::get(parent);

    parentPrivate->pending->subsurface.position[q] = QPoint(x, y);
    parentPrivate->pending->subsurfacePositionChanged = true;
}

void SubSurfaceInterfacePrivate::subsurface_place_above(Resource *resource, struct ::wl_resource *sibling_resource)
{
    SurfaceInterface *sibling = SurfaceInterface::get(sibling_resource);
    if (!sibling) {
        wl_resource_post_error(resource->handle, error_bad_surface, "no sibling");
        return;
    }
    if (!parent) {
        wl_resource_post_error(resource->handle, error_bad_surface, "no parent");
        return;
    }

    SurfaceInterfacePrivate *parentPrivate = SurfaceInterfacePrivate::get(parent);
    if (!parentPrivate->raiseChild(q, sibling)) {
        wl_resource_post_error(resource->handle, error_bad_surface, "incorrect sibling");
    }
}

void SubSurfaceInterfacePrivate::subsurface_place_below(Resource *resource, struct ::wl_resource *sibling_resource)
{
    SurfaceInterface *sibling = SurfaceInterface::get(sibling_resource);
    if (!sibling) {
        wl_resource_post_error(resource->handle, error_bad_surface, "no sibling");
        return;
    }
    if (!parent) {
        wl_resource_post_error(resource->handle, error_bad_surface, "no parent");
        return;
    }

    SurfaceInterfacePrivate *parentPrivate = SurfaceInterfacePrivate::get(parent);
    if (!parentPrivate->lowerChild(q, sibling)) {
        wl_resource_post_error(resource->handle, error_bad_surface, "incorrect sibling");
    }
}

void SubSurfaceInterfacePrivate::subsurface_set_sync(Resource *)
{
    if (mode == SubSurfaceInterface::Mode::Synchronized) {
        return;
    }
    mode = SubSurfaceInterface::Mode::Synchronized;
    Q_EMIT q->modeChanged(SubSurfaceInterface::Mode::Synchronized);
}

void SubSurfaceInterfacePrivate::subsurface_set_desync(Resource *)
{
    if (mode == SubSurfaceInterface::Mode::Desynchronized) {
        return;
    }
    mode = SubSurfaceInterface::Mode::Desynchronized;
    if (!q->isSynchronized()) {
        auto surfacePrivate = SurfaceInterfacePrivate::get(surface);
        while (!locks.isEmpty()) {
            SubSurfaceStateLock lock = locks.takeFirst();
            surfacePrivate->unlockState(lock.serial);
        }
    }
    Q_EMIT q->modeChanged(SubSurfaceInterface::Mode::Desynchronized);
}

void SubSurfaceInterfacePrivate::parentApplyState(quint32 serial)
{
    auto parentPrivate = SurfaceInterfacePrivate::get(parent);
    if (parentPrivate->current->subsurfacePositionChanged) {
        const QPoint &pos = parentPrivate->current->subsurface.position[q];
        if (position != pos) {
            position = pos;
            Q_EMIT q->positionChanged(pos);
        }
    }

    if (mode == SubSurfaceInterface::Mode::Synchronized) {
        auto surfacePrivate = SurfaceInterfacePrivate::get(surface);
        while (!locks.isEmpty() && locks[0].parentSerial == serial) {
            SubSurfaceStateLock lock = locks.takeFirst();
            surfacePrivate->unlockState(lock.serial);
        }
    }
}

SubSurfaceInterface::SubSurfaceInterface(SurfaceInterface *surface, SurfaceInterface *parent, wl_resource *resource)
    : d(new SubSurfaceInterfacePrivate(this, surface, parent, resource))
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    SurfaceInterfacePrivate *parentPrivate = SurfaceInterfacePrivate::get(parent);
    surfacePrivate->subSurface = this;
    parentPrivate->addChild(this);

    connect(surface, &SurfaceInterface::destroyed, this, [this]() {
        delete this;
    });
}

SubSurfaceInterface::~SubSurfaceInterface()
{
    if (d->parent) {
        SurfaceInterfacePrivate *parentPrivate = SurfaceInterfacePrivate::get(d->parent);
        parentPrivate->removeChild(this);
    }
    if (d->surface) {
        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(d->surface);
        surfacePrivate->subSurface = nullptr;
    }
}

SurfaceRole *SubSurfaceInterface::role()
{
    static SurfaceRole role(QByteArrayLiteral("wl_subsurface"));
    return &role;
}

QPoint SubSurfaceInterface::position() const
{
    return d->position;
}

SurfaceInterface *SubSurfaceInterface::surface() const
{
    return d->surface;
}

SurfaceInterface *SubSurfaceInterface::parentSurface() const
{
    return d->parent;
}

SubSurfaceInterface::Mode SubSurfaceInterface::mode() const
{
    return d->mode;
}

bool SubSurfaceInterface::isSynchronized() const
{
    if (d->mode == Mode::Synchronized) {
        return true;
    }
    if (!d->parent) {
        // that shouldn't happen, but let's assume false
        return false;
    }
    if (d->parent->subSurface()) {
        // follow parent's mode
        return d->parent->subSurface()->isSynchronized();
    }
    // parent is no subsurface, thus parent is in desync mode and this surface is in desync mode
    return false;
}

SurfaceInterface *SubSurfaceInterface::mainSurface() const
{
    if (!d->parent) {
        return nullptr;
    }
    SurfaceInterfacePrivate *parentPrivate = SurfaceInterfacePrivate::get(d->parent);
    if (parentPrivate->subSurface) {
        return parentPrivate->subSurface->mainSurface();
    }
    return d->parent;
}

void SubSurfaceInterface::commit()
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(d->surface);
    if (isSynchronized()) {
        const quint32 serial = surfacePrivate->lockState(surfacePrivate->pending.get());
        const quint32 parentSerial = SurfaceInterfacePrivate::get(d->parent)->pending->serial;
        d->locks.append(SubSurfaceStateLock{
            .serial = serial,
            .parentSerial = parentSerial,
        });
    } else {
        while (!d->locks.isEmpty()) {
            SubSurfaceStateLock lock = d->locks.takeFirst();
            surfacePrivate->unlockState(lock.serial);
        }
    }
}

} // namespace KWaylandServer

#include "moc_subcompositor_interface.cpp"
