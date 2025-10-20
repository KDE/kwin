/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "subcompositor.h"
#include "display.h"
#include "subsurface_p.h"
#include "surface_p.h"
#include "transaction.h"

namespace KWin
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

    parentPrivate->pending->subsurface.position[q] = QPoint(x, y) / parent->clientToCompositorScale();
    parentPrivate->pending->committed |= SurfaceState::Field::SubsurfacePosition;
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
        q->parentDesynchronized();
    }
    Q_EMIT q->modeChanged(SubSurfaceInterface::Mode::Desynchronized);
}

SubSurfaceInterface::SubSurfaceInterface(SurfaceInterface *surface, SurfaceInterface *parent, wl_resource *resource)
    : d(new SubSurfaceInterfacePrivate(this, surface, parent, resource))
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    SurfaceInterfacePrivate *parentPrivate = SurfaceInterfacePrivate::get(parent);
    surfacePrivate->subsurface.handle = this;
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
        surfacePrivate->subsurface.handle = nullptr;
        surfacePrivate->subsurface.transaction.reset();
    }
}

SurfaceRole *SubSurfaceInterface::role()
{
    static SurfaceRole role(QByteArrayLiteral("wl_subsurface"));
    return &role;
}

QPointF SubSurfaceInterface::position() const
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
    if (parentPrivate->subsurface.handle) {
        return parentPrivate->subsurface.handle->mainSurface();
    }
    return d->parent;
}

void SubSurfaceInterface::parentDesynchronized()
{
    if (d->mode == SubSurfaceInterface::Mode::Synchronized) {
        return;
    }

    auto surfacePrivate = SurfaceInterfacePrivate::get(d->surface);
    if (surfacePrivate->subsurface.transaction) {
        surfacePrivate->subsurface.transaction->commit();
        surfacePrivate->subsurface.transaction.release();
    }

    const auto below = d->surface->below();
    for (SubSurfaceInterface *child : below) {
        child->parentDesynchronized();
    }

    const auto above = d->surface->above();
    for (SubSurfaceInterface *child : above) {
        child->parentDesynchronized();
    }
}

void SubSurfaceInterface::parentApplyState()
{
    auto parentPrivate = SurfaceInterfacePrivate::get(d->parent);
    if (parentPrivate->current->committed & SurfaceState::Field::SubsurfacePosition) {
        const QPointF &pos = parentPrivate->current->subsurface.position[this];
        if (d->position != pos) {
            d->position = pos;
            Q_EMIT positionChanged(pos);
        }
    }
}

} // namespace KWin

#include "moc_subcompositor.cpp"
