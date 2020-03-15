/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
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
    static const quint32 s_version;
};

const quint32 SubCompositorInterface::Private::s_version = 1;

#ifndef K_DOXYGEN
const struct wl_subcompositor_interface SubCompositorInterface::Private::s_interface = {
    destroyCallback,
    subsurfaceCallback
};
#endif

SubCompositorInterface::Private::Private(SubCompositorInterface *q, Display *d)
    : Global::Private(d, &wl_subcompositor_interface, s_version)
    , q(q)
{
}

void SubCompositorInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&wl_subcompositor_interface, qMin(version, s_version), id);
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
    wl_resource_destroy(resource);
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
    SubSurfaceInterface *s = new SubSurfaceInterface(q, resource);
    s->d_func()->create(display->getConnection(client), wl_resource_get_version(resource), id, surface, parentSurface);
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

#ifndef K_DOXYGEN
const struct wl_subsurface_interface SubSurfaceInterface::Private::s_interface = {
    resourceDestroyedCallback,
    setPositionCallback,
    placeAboveCallback,
    placeBelowCallback,
    setSyncCallback,
    setDeSyncCallback
};
#endif

SubSurfaceInterface::Private::Private(SubSurfaceInterface *q, SubCompositorInterface *compositor, wl_resource *parentResource)
    : Resource::Private(q, compositor, parentResource, &wl_subsurface_interface, &s_interface)
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

void SubSurfaceInterface::Private::create(ClientConnection *client, quint32 version, quint32 id, SurfaceInterface *s, SurfaceInterface *p)
{
    create(client, version, id);
    if (!resource) {
        return;
    }
    surface = s;
    parent = p;
    Q_Q(SubSurfaceInterface);
    surface->d_func()->subSurface = QPointer<SubSurfaceInterface>(q);
    // copy current state to subSurfacePending state
    // it's the reference for all new pending state which needs to be committed
    surface->d_func()->subSurfacePending = surface->d_func()->current;
    surface->d_func()->subSurfacePending.blurIsSet = false;
    surface->d_func()->subSurfacePending.bufferIsSet = false;
    surface->d_func()->subSurfacePending.childrenChanged = false;
    surface->d_func()->subSurfacePending.contrastIsSet = false;
    surface->d_func()->subSurfacePending.callbacks.clear();
    surface->d_func()->subSurfacePending.inputIsSet = false;
    surface->d_func()->subSurfacePending.inputIsInfinite = true;
    surface->d_func()->subSurfacePending.opaqueIsSet = false;
    surface->d_func()->subSurfacePending.shadowIsSet = false;
    surface->d_func()->subSurfacePending.slideIsSet = false;
    parent->d_func()->addChild(QPointer<SubSurfaceInterface>(q));

    QObject::connect(surface.data(), &QObject::destroyed, q,
        [this] {
            // from spec: "If the wl_surface associated with the wl_subsurface is destroyed,
            // the wl_subsurface object becomes inert. Note, that destroying either object
            // takes effect immediately."
            if (parent) {
                Q_Q(SubSurfaceInterface);
                reinterpret_cast<SurfaceInterface::Private*>(parent->d.data())->removeChild(QPointer<SubSurfaceInterface>(q));
            }
        }
    );
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
    if (surface) {
        surface->d_func()->commitSubSurface();
    }
}

void SubSurfaceInterface::Private::setPositionCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y)
{
    Q_UNUSED(client)
    // TODO: is this a fixed position?
    cast<Private>(resource)->setPosition(QPoint(x, y));
}

void SubSurfaceInterface::Private::setPosition(const QPoint &p)
{
    Q_Q(SubSurfaceInterface);
    if (!q->isSynchronized()) {
        // workaround for https://bugreports.qt.io/browse/QTBUG-52118
        // apply directly as Qt doesn't commit the parent surface
        pos = p;
        emit q->positionChanged(pos);
        return;
    }
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
    if (m == Mode::Desynchronized && (!parent->subSurface() || !parent->subSurface()->isSynchronized())) {
        // no longer synchronized, this is like calling commit
        if (surface) {
            surface->d_func()->commit();
            surface->d_func()->commitSubSurface();
        }
    }
    mode = m;
    Q_Q(SubSurfaceInterface);
    emit q->modeChanged(m);
}

SubSurfaceInterface::SubSurfaceInterface(SubCompositorInterface *parent, wl_resource *parentResource)
    : Resource(new Private(this, parent, parentResource))
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
    // TODO: remove with ABI break (KF6)
    Q_D();
    return d->surface;
}

QPointer<SurfaceInterface> SubSurfaceInterface::surface() const
{
    Q_D();
    return d->surface;
}

QPointer<SurfaceInterface> SubSurfaceInterface::parentSurface()
{
    // TODO: remove with ABI break (KF6)
    Q_D();
    return d->parent;
}

QPointer<SurfaceInterface> SubSurfaceInterface::parentSurface() const
{
    Q_D();
    return d->parent;
}

SubSurfaceInterface::Mode SubSurfaceInterface::mode() const
{
    Q_D();
    return d->mode;
}

bool SubSurfaceInterface::isSynchronized() const
{
    Q_D();
    if (d->mode == Mode::Synchronized) {
        return true;
    }
    if (d->parent.isNull()) {
        // that shouldn't happen, but let's assume false
        return false;
    }
    if (!d->parent->subSurface().isNull()) {
        // follow parent's mode
        return d->parent->subSurface()->isSynchronized();
    }
    // parent is no subsurface, thus parent is in desync mode and this surface is in desync mode
    return false;
}

QPointer<SurfaceInterface> SubSurfaceInterface::mainSurface() const
{
    Q_D();
    if (!d->parent) {
        return QPointer<SurfaceInterface>();
    }
    if (d->parent->d_func()->subSurface) {
        return d->parent->d_func()->subSurface->mainSurface();
    }
    return d->parent;
}

SubSurfaceInterface::Private *SubSurfaceInterface::d_func() const
{
    return reinterpret_cast<SubSurfaceInterface::Private*>(d.data());
}

}
}
