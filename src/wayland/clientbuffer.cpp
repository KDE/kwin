/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "clientbuffer.h"
#include "clientbuffer_p.h"

#include "qwayland-server-wayland.h"

namespace KWaylandServer
{
ClientBuffer::ClientBuffer(ClientBufferPrivate &dd)
    : d_ptr(&dd)
{
}

ClientBuffer::ClientBuffer(wl_resource *resource, ClientBufferPrivate &dd)
    : d_ptr(&dd)
{
    initialize(resource);
}

ClientBuffer::~ClientBuffer()
{
}

void ClientBuffer::initialize(wl_resource *resource)
{
    Q_D(ClientBuffer);
    d->resource = resource;
}

wl_resource *ClientBuffer::resource() const
{
    Q_D(const ClientBuffer);
    return d->resource;
}

bool ClientBuffer::isReferenced() const
{
    Q_D(const ClientBuffer);
    return d->refCount > 0;
}

bool ClientBuffer::isDestroyed() const
{
    Q_D(const ClientBuffer);
    return d->isDestroyed;
}

void ClientBuffer::ref()
{
    Q_D(ClientBuffer);
    d->refCount++;
}

void ClientBuffer::unref()
{
    Q_D(ClientBuffer);
    Q_ASSERT(d->refCount > 0);
    --d->refCount;
    if (!isReferenced()) {
        if (isDestroyed()) {
            delete this;
        } else {
            wl_buffer_send_release(d->resource);
        }
    }
}

void ClientBuffer::markAsDestroyed()
{
    Q_D(ClientBuffer);
    if (!isReferenced()) {
        delete this;
    } else {
        d->resource = nullptr;
        d->isDestroyed = true;
    }
}

} // namespace KWaylandServer
