/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "resource.h"
#include "resource_p.h"
#include "clientconnection.h"

#include <wayland-server.h>

namespace KWaylandServer
{

QList<Resource::Private*> Resource::Private::s_allResources;

Resource::Private::Private(Resource *q, Global *g, wl_resource *parentResource, const wl_interface *interface, const void *implementation)
    : parentResource(parentResource)
    , global(g)
    , q(q)
    , m_interface(interface)
    , m_interfaceImplementation(implementation)
{
    s_allResources << this;
}

Resource::Private::~Private()
{
    s_allResources.removeAll(this);
    if (resource) {
        wl_resource_destroy(resource);
    }
}

void Resource::Private::create(ClientConnection *c, quint32 version, quint32 id)
{
    Q_ASSERT(!resource);
    Q_ASSERT(!client);
    client = c;
    resource = client->createResource(m_interface, version, id);
    if (!resource) {
        return;
    }
    wl_resource_set_implementation(resource, m_interfaceImplementation, this, unbind);
}

void Resource::Private::unbind(wl_resource *r)
{
    Private *p = cast<Private>(r);
    emit p->q->aboutToBeUnbound();
    p->resource = nullptr;
    emit p->q->unbound();
    p->q->deleteLater();
}


void Resource::Private::resourceDestroyedCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    wl_resource_destroy(resource);
}

Resource::Resource(Resource::Private *d, QObject *parent)
    : QObject(parent)
    , d(d)
{
}

Resource::~Resource() = default;

void Resource::create(ClientConnection *client, quint32 version, quint32 id)
{
    d->create(client, version, id);
}

ClientConnection *Resource::client()
{
    return d->client;
}

Global *Resource::global()
{
    return d->global;
}

wl_resource *Resource::resource()
{
    return d->resource;
}

wl_resource *Resource::parentResource() const
{
    return d->parentResource;
}

quint32 Resource::id() const
{
    if (!d->resource) {
        return 0;
    }
    return wl_resource_get_id(d->resource);
}

}
