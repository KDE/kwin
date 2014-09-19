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
#include "buffer_interface.h"
#include "compositor_interface.h"
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class SurfaceInterface::Private
{
public:
    struct State {
        QRegion damage = QRegion();
        QRegion opaque = QRegion();
        QRegion input = QRegion();
        qint32 scale = 1;
        OutputInterface::Transform transform = OutputInterface::Transform::Normal;
        QList<wl_resource*> callbacks = QList<wl_resource*>();
        QPoint offset = QPoint();
        BufferInterface *buffer = nullptr;
    };
    Private(SurfaceInterface *q, CompositorInterface *c);
    ~Private();

    void create(wl_client *client, quint32 version, quint32 id);
    void destroy();

    static SurfaceInterface *get(wl_resource *native);

    CompositorInterface *compositor;
    wl_resource *surface = nullptr;
    wl_client *client = nullptr;
    State current;
    State pending;

private:
    void commit();
    void damage(const QRect &rect);
    void setScale(qint32 scale);
    void setTransform(OutputInterface::Transform transform);
    void addFrameCallback(uint32_t callback);
    void attachBuffer(wl_resource *buffer, const QPoint &offset);

    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    static void unbind(wl_resource *r);
    static void destroyFrameCallback(wl_resource *r);

    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void attachCallback(wl_client *client, wl_resource *resource, wl_resource *buffer, int32_t sx, int32_t sy);
    static void damageCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height);
    static void frameCallaback(wl_client *client, wl_resource *resource, uint32_t callback);
    static void opaqueRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region);
    static void inputRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region);
    static void commitCallback(wl_client *client, wl_resource *resource);
    // since version 2
    static void bufferTransformCallback(wl_client *client, wl_resource *resource, int32_t transform);
    // since version 3
    static void bufferScaleCallback(wl_client *client, wl_resource *resource, int32_t scale);

    SurfaceInterface *q;
    static const struct wl_surface_interface s_interface;
    static QList<SurfaceInterface::Private*> s_allSurfaces;
};

QList<SurfaceInterface::Private*> SurfaceInterface::Private::s_allSurfaces;

SurfaceInterface::Private::Private(SurfaceInterface *q, CompositorInterface *c)
    : compositor(c)
    , q(q)
{
    s_allSurfaces << this;
}

SurfaceInterface::Private::~Private()
{
    destroy();
    s_allSurfaces.removeAll(this);
}

SurfaceInterface *SurfaceInterface::Private::get(wl_resource *native)
{
    auto it = std::find_if(s_allSurfaces.constBegin(), s_allSurfaces.constEnd(), [native](Private *s) {
        return s->surface == native;
    });
    if (it == s_allSurfaces.constEnd()) {
        return nullptr;
    }
    return (*it)->q;
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
    : QObject(/*parent*/)
    , d(new Private(this, parent))
{
}

SurfaceInterface::~SurfaceInterface() = default;

void SurfaceInterface::create(wl_client *client, quint32 version, quint32 id)
{
    d->create(client, version, id);
}

void SurfaceInterface::Private::create(wl_client *c, quint32 version, quint32 id)
{
    Q_ASSERT(!surface);
    Q_ASSERT(!client);
    client = c;
    surface = wl_resource_create(client, &wl_surface_interface, version, id);
    if (!surface) {
        return;
    }
    wl_resource_set_implementation(surface, &s_interface, this, unbind);
}

void SurfaceInterface::frameRendered(quint32 msec)
{
    // notify all callbacks
    while (!d->current.callbacks.isEmpty()) {
        wl_resource *r = d->current.callbacks.takeFirst();
        wl_callback_send_done(r, msec);
        wl_resource_destroy(r);
    }
}

void SurfaceInterface::Private::unbind(wl_resource *r)
{
    auto s = cast(r);
    s->surface = nullptr;
    s->q->deleteLater();
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
    if (surface) {
        wl_resource_destroy(surface);
        surface = nullptr;
    }
}

void SurfaceInterface::Private::commit()
{
    for (wl_resource *c : current.callbacks) {
        wl_resource_destroy(c);
    }
    const bool opaqueRegionChanged = current.opaque != pending.opaque;
    const bool inputRegionChanged = current.input != pending.input;
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
        wl_resource_post_no_memory(surface);
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
    pending.buffer = new BufferInterface(buffer, q);
}

void SurfaceInterface::Private::destroyFrameCallback(wl_resource *r)
{
    auto s = cast(r);
    s->current.callbacks.removeAll(r);
    s->pending.callbacks.removeAll(r);
}

void SurfaceInterface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    cast(resource)->q->deleteLater();
}

void SurfaceInterface::Private::attachCallback(wl_client *client, wl_resource *resource, wl_resource *buffer, int32_t sx, int32_t sy)
{
    Q_UNUSED(client)
    cast(resource)->attachBuffer(buffer, QPoint(sx, sy));
}

void SurfaceInterface::Private::damageCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    Q_UNUSED(client)
    cast(resource)->damage(QRect(x, y, width, height));
}

void SurfaceInterface::Private::frameCallaback(wl_client *client, wl_resource *resource, uint32_t callback)
{
    auto s = cast(resource);
    Q_ASSERT(client == s->client);
    s->addFrameCallback(callback);
}

void SurfaceInterface::Private::opaqueRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    Q_UNUSED(region)
    // TODO: implement me
}

void SurfaceInterface::Private::inputRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    Q_UNUSED(region)
    // TODO: implement me
}

void SurfaceInterface::Private::commitCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    cast(resource)->commit();
}

void SurfaceInterface::Private::bufferTransformCallback(wl_client *client, wl_resource *resource, int32_t transform)
{
    Q_UNUSED(client)
    cast(resource)->setTransform(OutputInterface::Transform(transform));
}

void SurfaceInterface::Private::bufferScaleCallback(wl_client *client, wl_resource *resource, int32_t scale)
{
    Q_UNUSED(client)
    cast(resource)->setScale(scale);
}

wl_resource *SurfaceInterface::surface() const
{
    return d->surface;
}

wl_client *SurfaceInterface::client() const
{
    return d->client;
}

QRegion SurfaceInterface::damage() const
{
    return d->current.damage;
}

QRegion SurfaceInterface::opaque() const
{
    return d->current.opaque;
}

QRegion SurfaceInterface::input() const
{
    return d->current.input;
}

qint32 SurfaceInterface::scale() const
{
    return d->current.scale;
}

OutputInterface::Transform SurfaceInterface::transform() const
{
    return d->current.transform;
}

BufferInterface *SurfaceInterface::buffer()
{
    return d->current.buffer;
}

QPoint SurfaceInterface::offset() const
{
    return d->current.offset;
}

SurfaceInterface *SurfaceInterface::get(wl_resource *native)
{
    return Private::get(native);
}

}
}
