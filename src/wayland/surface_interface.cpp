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

namespace KWin
{
namespace WaylandServer
{

const struct wl_surface_interface SurfaceInterface::s_interface = {
    SurfaceInterface::destroyCallback,
    SurfaceInterface::attachCallback,
    SurfaceInterface::damageCallback,
    SurfaceInterface::frameCallaback,
    SurfaceInterface::opaqueRegionCallback,
    SurfaceInterface::inputRegionCallback,
    SurfaceInterface::commitCallback,
    SurfaceInterface::bufferTransformCallback,
    SurfaceInterface::bufferScaleCallback
};

SurfaceInterface::SurfaceInterface(CompositorInterface *parent)
    : QObject(parent)
    , m_compositor(parent)
    , m_surface(nullptr)
    , m_client(nullptr)
{
}

SurfaceInterface::~SurfaceInterface()
{
    destroy();
}

void SurfaceInterface::create(wl_client *client, quint32 version, quint32 id)
{
    Q_ASSERT(!m_surface);
    Q_ASSERT(!m_client);
    m_client = client;
    m_surface = wl_resource_create(client, &wl_surface_interface, version, id);
    if (!m_surface) {
        return;
    }
    wl_resource_set_implementation(m_surface, &SurfaceInterface::s_interface, this, SurfaceInterface::unbind);
}

void SurfaceInterface::frameRendered(quint32 msec)
{
    // notify all callbacks
    while (!m_current.callbacks.isEmpty()) {
        wl_resource *r = m_current.callbacks.takeFirst();
        wl_callback_send_done(r, msec);
        wl_resource_destroy(r);
    }
}

void SurfaceInterface::unbind(wl_resource *r)
{
    SurfaceInterface *s = SurfaceInterface::cast(r);
    s->m_surface = nullptr;
    s->deleteLater();
}

void SurfaceInterface::destroy()
{
    for (wl_resource *c : m_current.callbacks) {
        wl_resource_destroy(c);
    }
    for (wl_resource *c : m_pending.callbacks) {
        wl_resource_destroy(c);
    }
    if (m_current.buffer) {
        m_current.buffer->unref();
    }
    if (m_surface) {
        wl_resource_destroy(m_surface);
        m_surface = nullptr;
    }
}

void SurfaceInterface::commit()
{
    for (wl_resource *c : m_current.callbacks) {
        wl_resource_destroy(c);
    }
    const bool opaqueRegionChanged = m_current.opaque != m_pending.opaque;
    const bool inputRegionChanged = m_current.input != m_pending.input;
    const bool scaleFactorChanged = m_current.scale != m_pending.scale;
    const bool transformFactorChanged = m_current.transform != m_pending.transform;
    if (m_current.buffer) {
        m_current.buffer->unref();
    }
    if (m_pending.buffer) {
        m_pending.buffer->ref();
    }
    // copy values
    m_current = m_pending;
    m_pending = State{};
    if (opaqueRegionChanged) {
        emit opaqueChanged(m_current.opaque);
    }
    if (inputRegionChanged) {
        emit inputChanged(m_current.input);
    }
    if (scaleFactorChanged) {
        emit scaleChanged(m_current.scale);
    }
    if (transformFactorChanged) {
        emit transformChanged(m_current.transform);
    }
    if (!m_current.damage.isEmpty()) {
        emit damaged(m_current.damage);
    }
}

void SurfaceInterface::damage(const QRect &rect)
{
    // TODO: documentation says we need to remove the parts outside of the surface
    m_pending.damage = m_pending.damage.united(rect);
}

void SurfaceInterface::setScale(qint32 scale)
{
    m_pending.scale = scale;
}

void SurfaceInterface::setTransform(OutputInterface::Transform transform)
{
    m_pending.transform = transform;
}

void SurfaceInterface::addFrameCallback(uint32_t callback)
{
    wl_resource *r = wl_resource_create(m_client, &wl_callback_interface, 1, callback);
    if (!r) {
        wl_resource_post_no_memory(m_surface);
        return;
    }
    wl_resource_set_implementation(r, nullptr, this, destroyFrameCallback);
    m_pending.callbacks << r;
}

void SurfaceInterface::attachBuffer(wl_resource *buffer, const QPoint &offset)
{
    m_pending.offset = offset;
    if (m_pending.buffer) {
        delete m_pending.buffer;
    }
    m_pending.buffer = new BufferInterface(buffer, this);
}

void SurfaceInterface::destroyFrameCallback(wl_resource *r)
{
    SurfaceInterface *s = SurfaceInterface::cast(r);
    s->m_current.callbacks.removeAll(r);
    s->m_pending.callbacks.removeAll(r);
}

void SurfaceInterface::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    SurfaceInterface::cast(resource)->deleteLater();
}

void SurfaceInterface::attachCallback(wl_client *client, wl_resource *resource, wl_resource *buffer, int32_t sx, int32_t sy)
{
    Q_UNUSED(client)
    SurfaceInterface::cast(resource)->attachBuffer(buffer, QPoint(sx, sy));
}

void SurfaceInterface::damageCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    Q_UNUSED(client)
    SurfaceInterface::cast(resource)->damage(QRect(x, y, width, height));
}

void SurfaceInterface::frameCallaback(wl_client *client, wl_resource *resource, uint32_t callback)
{
    SurfaceInterface *s = SurfaceInterface::cast(resource);
    Q_ASSERT(client == s->m_client);
    s->addFrameCallback(callback);
}

void SurfaceInterface::opaqueRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    Q_UNUSED(region)
    // TODO: implement me
}

void SurfaceInterface::inputRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    Q_UNUSED(region)
    // TODO: implement me
}

void SurfaceInterface::commitCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    SurfaceInterface::cast(resource)->commit();
}

void SurfaceInterface::bufferTransformCallback(wl_client *client, wl_resource *resource, int32_t transform)
{
    Q_UNUSED(client)
    SurfaceInterface::cast(resource)->setTransform(OutputInterface::Transform(transform));
}

void SurfaceInterface::bufferScaleCallback(wl_client *client, wl_resource *resource, int32_t scale)
{
    Q_UNUSED(client)
    SurfaceInterface::cast(resource)->setScale(scale);
}

}
}
