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
#include "clientconnection.h"
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

SurfaceInterface::Private::Private(SurfaceInterface *q, CompositorInterface *c, wl_resource *parentResource)
    : Resource::Private(q, c, parentResource, &wl_surface_interface, &s_interface)
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

void SurfaceInterface::Private::setShadow(const QPointer<ShadowInterface> &shadow)
{
    pending.shadow = shadow;
    pending.shadowIsSet = true;
}

void SurfaceInterface::Private::setBlur(const QPointer<BlurInterface> &blur)
{
    pending.blur = blur;
    pending.blurIsSet = true;
}

void SurfaceInterface::Private::setContrast(const QPointer<ContrastInterface> &contrast)
{
    pending.contrast = contrast;
    pending.contrastIsSet = true;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
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
#endif

SurfaceInterface::SurfaceInterface(CompositorInterface *parent, wl_resource *parentResource)
    : Resource(new Private(this, parent, parentResource))
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
    const bool bufferChanged = pending.bufferIsSet;
    const bool opaqueRegionChanged = pending.opaqueIsSet;
    const bool inputRegionChanged = pending.inputIsSet;
    const bool scaleFactorChanged = current.scale != pending.scale;
    const bool transformFactorChanged = current.transform != pending.transform;
    const bool shadowChanged = pending.shadowIsSet;
    const bool blurChanged = pending.blurIsSet;
    const bool contrastChanged = pending.contrastIsSet;
    bool sizeChanged = false;
    auto buffer = current.buffer;
    if (bufferChanged) {
        QSize oldSize;
        if (current.buffer) {
            oldSize = current.buffer->size();
            current.buffer->unref();
            QObject::disconnect(current.buffer, &BufferInterface::sizeChanged, q, &SurfaceInterface::sizeChanged);
        }
        if (pending.buffer) {
            pending.buffer->ref();
            QObject::connect(pending.buffer, &BufferInterface::sizeChanged, q, &SurfaceInterface::sizeChanged);
            const QSize newSize = pending.buffer->size();
            sizeChanged = newSize.isValid() && newSize != oldSize;
        }
        buffer = pending.buffer;
    }
    auto shadow = current.shadow;
    if (shadowChanged) {
        shadow = pending.shadow;
    }
    auto blur = current.blur;
    if (blurChanged) {
        blur = pending.blur;
    }
    auto contrast = current.contrast;
    if (contrastChanged) {
        contrast = pending.contrast;
    }
    QList<wl_resource*> callbacks = current.callbacks;
    callbacks.append(pending.callbacks);
    // copy values
    current = pending;
    current.buffer = buffer;
    current.callbacks = callbacks;
    current.shadow = shadow;
    current.blur = blur;
    current.contrast = contrast;
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
    if (bufferChanged) {
        if (!current.damage.isEmpty()) {
            const QRegion windowRegion = QRegion(0, 0, q->size().width(), q->size().height());
            if (!windowRegion.isEmpty()) {
                current.damage = windowRegion.intersected(current.damage);
            }
            emit q->damaged(current.damage);
        } else if (!current.buffer) {
            emit q->unmapped();
        }
    }
    if (sizeChanged) {
        emit q->sizeChanged();
    }
    if (shadowChanged) {
        emit q->shadowChanged();
    }
    if (blurChanged) {
        emit q->blurChanged();
    }
    if (contrastChanged) {
        emit q->contrastChanged();
    }
}

void SurfaceInterface::Private::damage(const QRect &rect)
{
    if (!pending.bufferIsSet || (pending.bufferIsSet && !pending.buffer)) {
        // TODO: should we send an error?
        return;
    }
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
    wl_resource *r = client->createResource(&wl_callback_interface, 1, callback);
    if (!r) {
        wl_resource_post_no_memory(resource);
        return;
    }
    wl_resource_set_implementation(r, nullptr, this, destroyFrameCallback);
    pending.callbacks << r;
}

void SurfaceInterface::Private::attachBuffer(wl_resource *buffer, const QPoint &offset)
{
    pending.bufferIsSet = true;
    pending.offset = offset;
    if (pending.buffer) {
        delete pending.buffer;
    }
    if (!buffer) {
        // got a null buffer, deletes content in next frame
        pending.buffer = nullptr;
        pending.damage = QRegion();
        return;
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
    wl_resource_destroy(resource);
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
    Q_ASSERT(client == *s->client);
    s->addFrameCallback(callback);
}

void SurfaceInterface::Private::opaqueRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);
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
    Q_ASSERT(client == *s->client);
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

SurfaceInterface *SurfaceInterface::get(quint32 id, const ClientConnection *client)
{
    return Private::get<SurfaceInterface>(id, client);
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

QSize SurfaceInterface::size() const
{
    Q_D();
    // TODO: apply transform and scale to the buffer size
    if (d->current.buffer) {
        return d->current.buffer->size();
    }
    return QSize();
}

QPointer< ShadowInterface > SurfaceInterface::shadow() const
{
    Q_D();
    return d->current.shadow;
}

QPointer< BlurInterface > SurfaceInterface::blur() const
{
    Q_D();
    return d->current.blur;
}

QPointer< ContrastInterface > SurfaceInterface::contrast() const
{
    Q_D();
    return d->current.contrast;
}

SurfaceInterface::Private *SurfaceInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}
