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

SubCompositorInterfacePrivate::SubCompositorInterfacePrivate(Display *display,
                                                             SubCompositorInterface *q)
    : QtWaylandServer::wl_subcompositor(*display, s_version)
    , q(q)
{
}

void SubCompositorInterfacePrivate::subcompositor_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void SubCompositorInterfacePrivate::subcompositor_get_subsurface(Resource *resource, uint32_t id,
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

    const SurfaceRole *surfaceRole = SurfaceRole::get(surface);
    if (surfaceRole) {
        wl_resource_post_error(resource->handle, error_bad_surface,
                               "the surface already has a role assigned %s",
                               surfaceRole->name().constData());
        return;
    }

    if (surface == parent) {
        wl_resource_post_error(resource->handle, error_bad_surface,
                               "wl_surface@%d cannot be its own parent",
                               wl_resource_get_id(surface_resource));
        return;
    }
    if (parent->subSurface() && parent->subSurface()->mainSurface() == surface) {
        wl_resource_post_error(resource->handle, error_bad_surface,
                               "wl_surface@%d is an ancestor of parent",
                               wl_resource_get_id(surface_resource));
        return;
    }

    wl_resource *subsurfaceResource = wl_resource_create(resource->client(), &wl_subsurface_interface,
                                                         resource->version(), id);
    if (!subsurfaceResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    emit q->subSurfaceCreated(new SubSurfaceInterface(surface, parent, subsurfaceResource));
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
    return subsurface->d.data();
}

SubSurfaceInterfacePrivate::SubSurfaceInterfacePrivate(SubSurfaceInterface *q,
                                                       SurfaceInterface *surface,
                                                       SurfaceInterface *parent,
                                                       ::wl_resource *resource)
    : SurfaceRole(surface, QByteArrayLiteral("wl_subsurface"))
    , QtWaylandServer::wl_subsurface(resource)
    , q(q)
    , surface(surface)
    , parent(parent)
{
}

void SubSurfaceInterfacePrivate::subsurface_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

void SubSurfaceInterfacePrivate::subsurface_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void SubSurfaceInterfacePrivate::subsurface_set_position(Resource *resource, int32_t x, int32_t y)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 2)
    if (!surface) {
        return;
    }
#endif
    Q_UNUSED(resource)
    if (pendingPosition == QPoint(x, y)) {
        return;
    }
    pendingPosition = QPoint(x, y);
    hasPendingPosition = true;
}

void SubSurfaceInterfacePrivate::subsurface_place_above(Resource *resource, struct ::wl_resource *sibling_resource)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 2)
    if (!surface) {
        return;
    }
#endif
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
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 2)
    if (!surface) {
        return;
    }
#endif
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
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 2)
    if (!surface) {
        return;
    }
#endif
    if (mode == SubSurfaceInterface::Mode::Synchronized) {
        return;
    }
    mode = SubSurfaceInterface::Mode::Synchronized;
    emit q->modeChanged(SubSurfaceInterface::Mode::Synchronized);
}

void SubSurfaceInterfacePrivate::subsurface_set_desync(Resource *)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 2)
    if (!surface) {
        return;
    }
#endif
    if (mode == SubSurfaceInterface::Mode::Desynchronized) {
        return;
    }
    mode = SubSurfaceInterface::Mode::Desynchronized;
    if (!q->isSynchronized()) {
        synchronizedCommit();
    }
    emit q->modeChanged(SubSurfaceInterface::Mode::Desynchronized);
}

void SubSurfaceInterfacePrivate::commit()
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);

    if (q->isSynchronized()) {
        commitToCache();
    } else {
        if (hasCacheState) {
            commitToCache();
            commitFromCache();
        } else {
            surfacePrivate->swapStates(&surfacePrivate->pending, &surfacePrivate->current, true);
        }

        const QList<SubSurfaceInterface *> children = surfacePrivate->current.children;
        for (SubSurfaceInterface *subsurface : children) {
            SubSurfaceInterfacePrivate *subsurfacePrivate = SubSurfaceInterfacePrivate::get(subsurface);
            subsurfacePrivate->parentCommit();
        }
    }
}

void SubSurfaceInterfacePrivate::parentCommit(bool synchronized)
{
    if (hasPendingPosition) {
        hasPendingPosition = false;
        position = pendingPosition;
        emit q->positionChanged(position);
    }

    if (synchronized || mode == SubSurfaceInterface::Mode::Synchronized) {
        synchronizedCommit();
    }
}

void SubSurfaceInterfacePrivate::synchronizedCommit()
{
    const SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    commitFromCache();

    const QList<SubSurfaceInterface *> children = surfacePrivate->current.children;
    for (SubSurfaceInterface *subsurface : children) {
        SubSurfaceInterfacePrivate *subsurfacePrivate = SubSurfaceInterfacePrivate::get(subsurface);
        subsurfacePrivate->parentCommit(true);
    }
}

void SubSurfaceInterfacePrivate::commitToCache()
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    surfacePrivate->swapStates(&surfacePrivate->pending, &surfacePrivate->cached, false);
    hasCacheState = true;
}

void SubSurfaceInterfacePrivate::commitFromCache()
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    surfacePrivate->swapStates(&surfacePrivate->cached, &surfacePrivate->current, true);
    hasCacheState = false;
}

SubSurfaceInterface::SubSurfaceInterface(SurfaceInterface *surface, SurfaceInterface *parent,
                                         wl_resource *resource)
    : d(new SubSurfaceInterfacePrivate(this, surface, parent, resource))
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    SurfaceInterfacePrivate *parentPrivate = SurfaceInterfacePrivate::get(parent);
    surfacePrivate->subSurface = this;
    parentPrivate->addChild(this);

    // copy current state to subSurfacePending state
    // it's the reference for all new pending state which needs to be committed
    surfacePrivate->cached = surfacePrivate->current;
    surfacePrivate->cached.blurIsSet = false;
    surfacePrivate->cached.bufferIsSet = false;
    surfacePrivate->cached.childrenChanged = false;
    surfacePrivate->cached.contrastIsSet = false;
    surfacePrivate->cached.frameCallbacks.clear();
    surfacePrivate->cached.inputIsSet = false;
    surfacePrivate->cached.opaqueIsSet = false;
    surfacePrivate->cached.shadowIsSet = false;
    surfacePrivate->cached.slideIsSet = false;

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 2)
    connect(surface, &SurfaceInterface::destroyed, this, [this]() {
        if (d->parent) {
            SurfaceInterfacePrivate *parentPrivate = SurfaceInterfacePrivate::get(d->parent);
            parentPrivate->removeChild(this);
        }
    });
#else
    connect(surface, &SurfaceInterface::destroyed, this, [this]() { delete this; });
#endif
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

} // namespace KWaylandServer
