/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "subcompositor_interface.h"
#include "subsurface_interface_p.h"
#include "global_p.h"
#include "display.h"
#include "surface_interface_p.h"
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

static const quint32 s_version = 1;

class SubCompositorInterface::Private : public Global::Private
{
public:
    Private(SubCompositorInterface *q, Display *d);

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void subsurface(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface, wl_resource *parent);

    static void unbind(wl_resource *resource);
    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void subsurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface, wl_resource *parent);

    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    SubCompositorInterface *q;
    static const struct wl_subcompositor_interface s_interface;
};

const struct wl_subcompositor_interface SubCompositorInterface::Private::s_interface = {
    destroyCallback,
    subsurfaceCallback
};

SubCompositorInterface::Private::Private(SubCompositorInterface *q, Display *d)
    : Global::Private(d, &wl_subcompositor_interface, s_version)
    , q(q)
{
}

void SubCompositorInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(client, &wl_subcompositor_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
}

void SubCompositorInterface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
}

void SubCompositorInterface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
}

void SubCompositorInterface::Private::subsurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface, wl_resource *sparent)
{
    cast(resource)->subsurface(client, resource, id, surface, sparent);
}

void SubCompositorInterface::Private::subsurface(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *nativeSurface, wl_resource *nativeParentSurface)
{
    Q_UNUSED(client)
    SurfaceInterface *surface = SurfaceInterface::get(nativeSurface);
    SurfaceInterface *parentSurface = SurfaceInterface::get(nativeParentSurface);
    if (!surface || !parentSurface) {
        wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "Surface or parent surface not found");
        return;
    }
    if (surface == parentSurface) {
        wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "Cannot become sub composite to same surface");
        return;
    }
    // TODO: add check that surface is not already used in an interface (e.g. Shell)
    // TODO: add check that parentSurface is not a child of surface
    SubSurfaceInterface *s = new SubSurfaceInterface(q);
    s->d_func()->create(client, wl_resource_get_version(resource), id, surface, parentSurface);
    if (!s->resource()) {
        wl_resource_post_no_memory(resource);
        delete s;
        return;
    }
    emit q->subSurfaceCreated(s);
}

SubCompositorInterface::SubCompositorInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

SubCompositorInterface::~SubCompositorInterface() = default;

const struct wl_subsurface_interface SubSurfaceInterface::Private::s_interface = {
    destroyCallback,
    setPositionCallback,
    placeAboveCallback,
    placeBelowCallback,
    setSyncCallback,
    setDeSyncCallback
};

SubSurfaceInterface::Private::Private(SubSurfaceInterface *q, SubCompositorInterface *compositor)
    : Resource::Private(q, compositor)
{
}

SubSurfaceInterface::Private::~Private()
{
    // no need to notify the surface as it's tracking a QPointer which will be reset automatically
    if (parent) {
        Q_Q(SubSurfaceInterface);
        reinterpret_cast<SurfaceInterface::Private*>(parent->d.data())->removeChild(QPointer<SubSurfaceInterface>(q));
    }
}

void SubSurfaceInterface::Private::create(wl_client *client, quint32 version, quint32 id)
{
    Q_ASSERT(!resource);
    resource = wl_resource_create(client, &wl_subsurface_interface, version, id);
    if (!resource) {
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
}

void SubSurfaceInterface::Private::create(wl_client *client, quint32 version, quint32 id, SurfaceInterface *s, SurfaceInterface *p)
{
    create(client, version, id);
    if (!resource) {
        return;
    }
    surface = s;
    parent = p;
    Q_Q(SubSurfaceInterface);
    surface->d_func()->subSurface = QPointer<SubSurfaceInterface>(q);
    parent->d_func()->addChild(QPointer<SubSurfaceInterface>(q));
}

void SubSurfaceInterface::Private::commit()
{
    if (scheduledPosChange) {
        scheduledPosChange = false;
        pos = scheduledPos;
        scheduledPos = QPoint();
        Q_Q(SubSurfaceInterface);
        emit q->positionChanged(pos);
    }
}

void SubSurfaceInterface::Private::unbind(wl_resource *r)
{
    auto s = cast<Private>(r);
    s->resource = nullptr;
    s->q_func()->deleteLater();
}

void SubSurfaceInterface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    cast<Private>(resource)->q_func()->deleteLater();
}

void SubSurfaceInterface::Private::setPositionCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y)
{
    Q_UNUSED(client)
    // TODO: is this a fixed position?
    cast<Private>(resource)->setPosition(QPoint(x, y));
}

void SubSurfaceInterface::Private::setPosition(const QPoint &p)
{
    if (scheduledPos == p) {
        return;
    }
    scheduledPos = p;
    scheduledPosChange = true;
}

void SubSurfaceInterface::Private::placeAboveCallback(wl_client *client, wl_resource *resource, wl_resource *sibling)
{
    Q_UNUSED(client)
    cast<Private>(resource)->placeAbove(SurfaceInterface::get(sibling));
}

void SubSurfaceInterface::Private::placeAbove(SurfaceInterface *sibling)
{
    if (parent.isNull()) {
        // TODO: raise error
        return;
    }
    Q_Q(SubSurfaceInterface);
    if (!parent->d_func()->raiseChild(QPointer<SubSurfaceInterface>(q), sibling)) {
        wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "Incorrect sibling");
    }
}

void SubSurfaceInterface::Private::placeBelowCallback(wl_client *client, wl_resource *resource, wl_resource *sibling)
{
    Q_UNUSED(client)
    cast<Private>(resource)->placeBelow(SurfaceInterface::get(sibling));
}

void SubSurfaceInterface::Private::placeBelow(SurfaceInterface *sibling)
{
    if (parent.isNull()) {
        // TODO: raise error
        return;
    }
    Q_Q(SubSurfaceInterface);
    if (!parent->d_func()->lowerChild(QPointer<SubSurfaceInterface>(q), sibling)) {
        wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "Incorrect sibling");
    }
}

void SubSurfaceInterface::Private::setSyncCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    cast<Private>(resource)->setMode(Mode::Synchronized);
}

void SubSurfaceInterface::Private::setDeSyncCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    cast<Private>(resource)->setMode(Mode::Desynchronized);
}

void SubSurfaceInterface::Private::setMode(Mode m)
{
    if (mode == m) {
        return;
    }
    mode = m;
    Q_Q(SubSurfaceInterface);
    emit q->modeChanged(m);
}

SubSurfaceInterface::SubSurfaceInterface(SubCompositorInterface *parent)
    : Resource(new Private(this, parent))
{
    Q_UNUSED(parent)
}

SubSurfaceInterface::~SubSurfaceInterface() = default;

QPoint SubSurfaceInterface::position() const
{
    Q_D();
    return d->pos;
}

QPointer<SurfaceInterface> SubSurfaceInterface::surface()
{
    Q_D();
    return d->surface;
}

QPointer<SurfaceInterface> SubSurfaceInterface::parentSurface()
{
    Q_D();
    return d->parent;
}

SubSurfaceInterface::Mode SubSurfaceInterface::mode() const
{
    Q_D();
    return d->mode;
}

SubSurfaceInterface::Private *SubSurfaceInterface::d_func() const
{
    return reinterpret_cast<SubSurfaceInterface::Private*>(d.data());
}

}
}
