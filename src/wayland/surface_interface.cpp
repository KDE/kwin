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
#include "surface_interface.h"
#include "surface_interface_p.h"
#include "buffer_interface.h"
#include "compositor_interface.h"
#include "region_interface.h"
#include "subcompositor_interface.h"
#include "subsurface_interface_p.h"
// Wayland
#include <wayland-server.h>
// std
#include <algorithm>

namespace KWayland
{
namespace Server
{

SurfaceInterface::Private::Private(SurfaceInterface *q, CompositorInterface *c)
    : Resource::Private(q, c, &wl_surface_interface, &s_interface)
{
}

SurfaceInterface::Private::~Private()
{
    destroy();
}

void SurfaceInterface::Private::addChild(QPointer< SubSurfaceInterface > subSurface)
{
    pending.children.append(subSurface);
}

void SurfaceInterface::Private::removeChild(QPointer< SubSurfaceInterface > subSurface)
{
    pending.children.removeAll(subSurface);
}

bool SurfaceInterface::Private::raiseChild(QPointer<SubSurfaceInterface> subsurface, SurfaceInterface *sibling)
{
    Q_Q(SurfaceInterface);
    auto it = std::find(pending.children.begin(), pending.children.end(), subsurface);
    if (it == pending.children.end()) {
        return false;
    }
    if (pending.children.count() == 1) {
        // nothing to do
        return true;
    }
    if (sibling == q) {
        // it's to the parent, so needs to become last item
        pending.children.append(*it);
        pending.children.erase(it);
        return true;
    }
    if (!sibling->subSurface()) {
        // not a sub surface
        return false;
    }
    auto siblingIt = std::find(pending.children.begin(), pending.children.end(), sibling->subSurface());
    if (siblingIt == pending.children.end() || siblingIt == it) {
        // not a sibling
        return false;
    }
    auto value = (*it);
    pending.children.erase(it);
    // find the iterator again
    siblingIt = std::find(pending.children.begin(), pending.children.end(), sibling->subSurface());
    pending.children.insert(++siblingIt, value);
    return true;
}

bool SurfaceInterface::Private::lowerChild(QPointer<SubSurfaceInterface> subsurface, SurfaceInterface *sibling)
{
    Q_Q(SurfaceInterface);
    auto it = std::find(pending.children.begin(), pending.children.end(), subsurface);
    if (it == pending.children.end()) {
        return false;
    }
    if (pending.children.count() == 1) {
        // nothing to do
        return true;
    }
    if (sibling == q) {
        // it's to the parent, so needs to become first item
        auto value = *it;
        pending.children.erase(it);
        pending.children.prepend(value);
        return true;
    }
    if (!sibling->subSurface()) {
        // not a sub surface
        return false;
    }
    auto siblingIt = std::find(pending.children.begin(), pending.children.end(), sibling->subSurface());
    if (siblingIt == pending.children.end() || siblingIt == it) {
        // not a sibling
        return false;
    }
    auto value = (*it);
    pending.children.erase(it);
    // find the iterator again
    siblingIt = std::find(pending.children.begin(), pending.children.end(), sibling->subSurface());
    pending.children.insert(siblingIt, value);
    return true;
}

const struct wl_surface_interface SurfaceInterface::Private::s_interface = {
    destroyCallback,
    attachCallback,
    damageCallback,
    frameCallaback,
    opaqueRegionCallback,
    inputRegionCallback,
    commitCallback,
    bufferTransformCallback,
    bufferScaleCallback
};

SurfaceInterface::SurfaceInterface(CompositorInterface *parent)
    : Resource(new Private(this, parent))
{
}

SurfaceInterface::~SurfaceInterface() = default;

void SurfaceInterface::frameRendered(quint32 msec)
{
    Q_D();
    // notify all callbacks
    while (!d->current.callbacks.isEmpty()) {
        wl_resource *r = d->current.callbacks.takeFirst();
        wl_callback_send_done(r, msec);
        wl_resource_destroy(r);
    }
}

void SurfaceInterface::Private::destroy()
{
    for (wl_resource *c : current.callbacks) {
        wl_resource_destroy(c);
    }
    for (wl_resource *c : pending.callbacks) {
        wl_resource_destroy(c);
    }
    if (current.buffer) {
        current.buffer->unref();
    }
    if (resource) {
        wl_resource_destroy(resource);
        resource = nullptr;
    }
}

void SurfaceInterface::Private::commit()
{
    Q_Q(SurfaceInterface);
    for (wl_resource *c : current.callbacks) {
        wl_resource_destroy(c);
    }
    const bool opaqueRegionChanged = pending.opaqueIsSet;
    const bool inputRegionChanged = pending.inputIsSet;
    const bool scaleFactorChanged = current.scale != pending.scale;
    const bool transformFactorChanged = current.transform != pending.transform;
    if (current.buffer) {
        current.buffer->unref();
    }
    if (pending.buffer) {
        pending.buffer->ref();
    }
    // copy values
    current = pending;
    pending = State{};
    pending.children = current.children;
    pending.input = current.input;
    pending.inputIsInfinite = current.inputIsInfinite;
    pending.opaque = current.opaque;
    // commit all subSurfaces to apply position changes
    for (auto it = current.children.constBegin(); it != current.children.constEnd(); ++it) {
        if (!(*it)) {
            continue;
        }
        (*it)->d_func()->commit();
    }
    if (opaqueRegionChanged) {
        emit q->opaqueChanged(current.opaque);
    }
    if (inputRegionChanged) {
        emit q->inputChanged(current.input);
    }
    if (scaleFactorChanged) {
        emit q->scaleChanged(current.scale);
    }
    if (transformFactorChanged) {
        emit q->transformChanged(current.transform);
    }
    if (!current.damage.isEmpty()) {
        emit q->damaged(current.damage);
    }
}

void SurfaceInterface::Private::damage(const QRect &rect)
{
    // TODO: documentation says we need to remove the parts outside of the surface
    pending.damage = pending.damage.united(rect);
}

void SurfaceInterface::Private::setScale(qint32 scale)
{
    pending.scale = scale;
}

void SurfaceInterface::Private::setTransform(OutputInterface::Transform transform)
{
    pending.transform = transform;
}

void SurfaceInterface::Private::addFrameCallback(uint32_t callback)
{
    wl_resource *r = wl_resource_create(client, &wl_callback_interface, 1, callback);
    if (!r) {
        wl_resource_post_no_memory(resource);
        return;
    }
    wl_resource_set_implementation(r, nullptr, this, destroyFrameCallback);
    pending.callbacks << r;
}

void SurfaceInterface::Private::attachBuffer(wl_resource *buffer, const QPoint &offset)
{
    pending.offset = offset;
    if (pending.buffer) {
        delete pending.buffer;
    }
    Q_Q(SurfaceInterface);
    pending.buffer = new BufferInterface(buffer, q);
    QObject::connect(pending.buffer, &BufferInterface::aboutToBeDestroyed, q,
        [this](BufferInterface *buffer) {
            if (pending.buffer == buffer) {
                pending.buffer = nullptr;
            }
            if (current.buffer == buffer) {
                current.buffer->unref();
                current.buffer = nullptr;
            }
        }
    );
}

void SurfaceInterface::Private::destroyFrameCallback(wl_resource *r)
{
    auto s = cast<Private>(r);
    s->current.callbacks.removeAll(r);
    s->pending.callbacks.removeAll(r);
}

void SurfaceInterface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    cast<Private>(resource)->q->deleteLater();
}

void SurfaceInterface::Private::attachCallback(wl_client *client, wl_resource *resource, wl_resource *buffer, int32_t sx, int32_t sy)
{
    Q_UNUSED(client)
    cast<Private>(resource)->attachBuffer(buffer, QPoint(sx, sy));
}

void SurfaceInterface::Private::damageCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    Q_UNUSED(client)
    cast<Private>(resource)->damage(QRect(x, y, width, height));
}

void SurfaceInterface::Private::frameCallaback(wl_client *client, wl_resource *resource, uint32_t callback)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == s->client);
    s->addFrameCallback(callback);
}

void SurfaceInterface::Private::opaqueRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == s->client);
    auto r = RegionInterface::get(region);
    s->setOpaque(r ? r->region() : QRegion());
}

void SurfaceInterface::Private::setOpaque(const QRegion &region)
{
    pending.opaqueIsSet = true;
    pending.opaque = region;
}

void SurfaceInterface::Private::inputRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == s->client);
    auto r = RegionInterface::get(region);
    s->setInput(r ? r->region() : QRegion(), !r);
}

void SurfaceInterface::Private::setInput(const QRegion &region, bool isInfinite)
{
    pending.inputIsSet = true;
    pending.inputIsInfinite = isInfinite;
    pending.input = region;
}

void SurfaceInterface::Private::commitCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    cast<Private>(resource)->commit();
}

void SurfaceInterface::Private::bufferTransformCallback(wl_client *client, wl_resource *resource, int32_t transform)
{
    Q_UNUSED(client)
    cast<Private>(resource)->setTransform(OutputInterface::Transform(transform));
}

void SurfaceInterface::Private::bufferScaleCallback(wl_client *client, wl_resource *resource, int32_t scale)
{
    Q_UNUSED(client)
    cast<Private>(resource)->setScale(scale);
}

QRegion SurfaceInterface::damage() const
{
    Q_D();
    return d->current.damage;
}

QRegion SurfaceInterface::opaque() const
{
    Q_D();
    return d->current.opaque;
}

QRegion SurfaceInterface::input() const
{
    Q_D();
    return d->current.input;
}

bool SurfaceInterface::inputIsInfitine() const
{
    Q_D();
    return d->current.inputIsInfinite;
}

qint32 SurfaceInterface::scale() const
{
    Q_D();
    return d->current.scale;
}

OutputInterface::Transform SurfaceInterface::transform() const
{
    Q_D();
    return d->current.transform;
}

BufferInterface *SurfaceInterface::buffer()
{
    Q_D();
    return d->current.buffer;
}

QPoint SurfaceInterface::offset() const
{
    Q_D();
    return d->current.offset;
}

SurfaceInterface *SurfaceInterface::get(wl_resource *native)
{
    return Private::get<SurfaceInterface>(native);
}

QList< QPointer< SubSurfaceInterface > > SurfaceInterface::childSubSurfaces() const
{
    Q_D();
    return d->current.children;
}

QPointer< SubSurfaceInterface > SurfaceInterface::subSurface() const
{
    Q_D();
    return d->subSurface;
}

SurfaceInterface::Private *SurfaceInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}
