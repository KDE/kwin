/****************************************************************************
Copyright 2016  Oleg Chernovskiy <kanedias@xaker.ru>

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
****************************************************************************/
#include "output_interface.h"
#include "remote_access_interface.h"
#include "remote_access_interface_p.h"
#include "display.h"
#include "global_p.h"
#include "resource_p.h"
#include "logging_p.h"

#include <wayland-remote-access-server-protocol.h>

#include <QHash>
#include <QMutableHashIterator>

#include <functional>

namespace KWayland
{
namespace Server
{

class BufferHandle::Private // @see gbm_import_fd_data
{
public:
    // Note that on client side received fd number will be different
    // and meaningful only for client process!
    // Thus we can use server-side fd as an implicit unique id
    qint32 fd = 0; ///< also internal buffer id for client
    quint32 width = 0;
    quint32 height = 0;
    quint32 stride = 0;
    quint32 format = 0;
};

BufferHandle::BufferHandle()
    : d(new Private)
{
}

BufferHandle::~BufferHandle()
{
}

void BufferHandle::setFd(qint32 fd)
{
    d->fd = fd;
}

qint32 BufferHandle::fd() const
{
    return d->fd;
}

void BufferHandle::setSize(quint32 width, quint32 height)
{
    d->width = width;
    d->height = height;
}

quint32 BufferHandle::width() const
{
    return d->width;
}

quint32 BufferHandle::height() const
{
    return d->height;
}

void BufferHandle::setStride(quint32 stride)
{
    d->stride = stride;
}


quint32 BufferHandle::stride() const
{
    return d->stride;
}

void BufferHandle::setFormat(quint32 format)
{
    d->format = format;
}

quint32 BufferHandle::format() const
{
    return d->format;
}

/**
 * @brief helper struct for manual reference counting.
 * automatic counting via QSharedPointer is no-go here as we hold strong reference in sentBuffers.
 */
struct BufferHolder
{
    const BufferHandle *buf;
    quint64 counter;
};
    
class RemoteAccessManagerInterface::Private : public Global::Private
{
public:
    Private(RemoteAccessManagerInterface *q, Display *d);
    virtual ~Private() override;

    /**
     * @brief Send buffer ready notification to all connected clients
     * @param output wl_output interface to determine which screen sent this buf
     * @param buf buffer containing GBM-related params
     */
    void sendBufferReady(const OutputInterface *output, const BufferHandle *buf);
    /**
     * @brief Release all bound buffers associated with this resource
     * @param resource one of bound clients
     */
    void release(wl_resource *resource);

    /**
     * Clients of this interface.
     * This may be screenshot app, video capture app,
     * remote control app etc.
     */
    QList<wl_resource *> clientResources;
private:
    // methods
    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }
    static void getBufferCallback(wl_client *client, wl_resource *resource, uint32_t buffer, int32_t internalBufId);
    static void releaseCallback(wl_client *client, wl_resource *resource);
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    /**
     * @brief Unreferences counter and frees buffer when it reaches zero
     * @param buf holder to decrease reference counter on
     * @return true if buffer was released, false otherwise
     */
    bool unref(BufferHolder &buf);

    // fields
    static const struct org_kde_kwin_remote_access_manager_interface s_interface;
    static const quint32 s_version;

    RemoteAccessManagerInterface *q;

    /**
     * Buffers that were sent but still not acked by server
     * Keys are fd numbers as they are unique
     **/
    QHash<qint32, BufferHolder> sentBuffers;
};

const quint32 RemoteAccessManagerInterface::Private::s_version = 1;

RemoteAccessManagerInterface::Private::Private(RemoteAccessManagerInterface *q, Display *d)
    : Global::Private(d, &org_kde_kwin_remote_access_manager_interface, s_version)
    , q(q)
{
}

void RemoteAccessManagerInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    // create new client resource
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&org_kde_kwin_remote_access_manager_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);

    // add newly created client resource to the list
    clientResources << resource;
}

void RemoteAccessManagerInterface::Private::sendBufferReady(const OutputInterface *output, const BufferHandle *buf)
{
    BufferHolder holder {buf, 0};
    // notify clients
    qCDebug(KWAYLAND_SERVER) << "Server buffer sent: fd" << buf->fd();
    for (auto res : clientResources) {
        auto client = wl_resource_get_client(res);
        auto boundScreens = output->clientResources(display->getConnection(client));

        // clients don't necessarily bind outputs
        if (boundScreens.isEmpty()) {
            return;
        }

        // no reason for client to bind wl_output multiple times, send only to first one
        org_kde_kwin_remote_access_manager_send_buffer_ready(res, buf->fd(), boundScreens[0]);
        holder.counter++;
    }
    // store buffer locally, clients will ask it later
    sentBuffers[buf->fd()] = holder;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct org_kde_kwin_remote_access_manager_interface RemoteAccessManagerInterface::Private::s_interface = {
    getBufferCallback,
    releaseCallback
};
#endif

void RemoteAccessManagerInterface::Private::getBufferCallback(wl_client *client, wl_resource *resource, uint32_t buffer, int32_t internalBufId)
{
    Private *p = cast(resource);

    // client asks for buffer we earlier announced, we must have it
    if (Q_UNLIKELY(!p->sentBuffers.contains(internalBufId))) { // no such buffer (?)
        wl_resource_post_no_memory(resource);
        return;
    }

    BufferHolder &bh = p->sentBuffers[internalBufId];
    auto rbuf = new RemoteBufferInterface(p->q, resource, bh.buf);
    rbuf->create(p->display->getConnection(client), wl_resource_get_version(resource), buffer);
    if (!rbuf->resource()) {
        wl_resource_post_no_memory(resource);
        delete rbuf;
        return;
    }

    QObject::connect(rbuf, &Resource::aboutToBeUnbound, p->q, [p, rbuf, resource, &bh] {
        if (!p->clientResources.contains(resource)) {
            // remote buffer destroy confirmed after client is already gone
            // all relevant buffers are already unreferenced
            return;
        }
        qCDebug(KWAYLAND_SERVER) << "Remote buffer returned, client" << wl_resource_get_id(resource)
                                                     << ", id" << rbuf->id()
                                                     << ", fd" << bh.buf->fd();
        if (p->unref(bh)) {
            p->sentBuffers.remove(bh.buf->fd());
        }
    });

    // send buffer params
    rbuf->passFd();
}

void RemoteAccessManagerInterface::Private::releaseCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client);
    unbind(resource);
}

bool RemoteAccessManagerInterface::Private::unref(BufferHolder &bh)
{
    bh.counter--;
    if (!bh.counter) {
        // no more clients using this buffer
        qCDebug(KWAYLAND_SERVER) << "Buffer released, fd" << bh.buf->fd();
        emit q->bufferReleased(bh.buf);
        return true;
    }

    return false;
}

void RemoteAccessManagerInterface::Private::unbind(wl_resource *resource)
{
    // we're unbinding, all sent buffers for this client are now effectively invalid
    Private *p = cast(resource);
    p->release(resource);
}

void RemoteAccessManagerInterface::Private::release(wl_resource *resource)
{
    // all holders should decrement their counter as one client is gone
    QMutableHashIterator<qint32, BufferHolder> itr(sentBuffers);
    while (itr.hasNext()) {
        BufferHolder &bh = itr.next().value();
        if (unref(bh)) {
            itr.remove();
        }
    }

    clientResources.removeAll(resource);
}

RemoteAccessManagerInterface::Private::~Private()
{
    // server deletes created interfaces, release all held buffers
    auto c = clientResources; // shadow copy
    for (auto res : c) {
        release(res);
    }
}

RemoteAccessManagerInterface::RemoteAccessManagerInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

void RemoteAccessManagerInterface::sendBufferReady(const OutputInterface *output, const BufferHandle *buf)
{
    Private *priv = reinterpret_cast<Private *>(d.data());
    priv->sendBufferReady(output, buf);
}

bool RemoteAccessManagerInterface::isBound() const
{
    Private *priv = reinterpret_cast<Private *>(d.data());
    return !priv->clientResources.isEmpty();
}

class RemoteBufferInterface::Private : public Resource::Private
{
public:
    Private(RemoteAccessManagerInterface *ram, RemoteBufferInterface *q, wl_resource *pResource, const BufferHandle *buf);
    ~Private();

    void passFd();

private:
    static const struct org_kde_kwin_remote_buffer_interface s_interface;

    const BufferHandle *wrapped;
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct org_kde_kwin_remote_buffer_interface RemoteBufferInterface::Private::s_interface = {
    resourceDestroyedCallback
};
#endif

RemoteBufferInterface::Private::Private(RemoteAccessManagerInterface *ram, RemoteBufferInterface *q, wl_resource *pResource, const BufferHandle *buf)
    : Resource::Private(q, ram, pResource, &org_kde_kwin_remote_buffer_interface, &s_interface), wrapped(buf)
{
}

RemoteBufferInterface::Private::~Private()
{
}

void RemoteBufferInterface::Private::passFd()
{
    org_kde_kwin_remote_buffer_send_gbm_handle(resource, wrapped->fd(),
            wrapped->width(), wrapped->height(), wrapped->stride(), wrapped->format());
}

RemoteBufferInterface::RemoteBufferInterface(RemoteAccessManagerInterface *ram, wl_resource *pResource, const BufferHandle *buf)
    : Resource(new Private(ram, this, pResource, buf), ram)
{
}

RemoteBufferInterface::Private *RemoteBufferInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}


void RemoteBufferInterface::passFd()
{
    d_func()->passFd();
}

}
}
