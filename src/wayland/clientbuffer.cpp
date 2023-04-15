/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "clientbuffer.h"
#include "clientbuffer_p.h"

#include <wayland-server-protocol.h>

namespace KWaylandServer
{
ClientBuffer::ClientBuffer(std::unique_ptr<ClientBufferPrivate> &&d)
    : d_ptr(std::move(d))
{
}

ClientBuffer::ClientBuffer(wl_resource *resource, std::unique_ptr<ClientBufferPrivate> &&d)
    : d_ptr(std::move(d))
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
    connect(this, &GraphicsBuffer::dropped, [d]() {
        d->resource = nullptr;
    });
    connect(this, &GraphicsBuffer::released, [d]() {
        wl_buffer_send_release(d->resource);
    });
}

wl_resource *ClientBuffer::resource() const
{
    Q_D(const ClientBuffer);
    return d->resource;
}

} // namespace KWaylandServer
