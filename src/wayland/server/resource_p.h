/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_RESOURCE_P_H
#define WAYLAND_SERVER_RESOURCE_P_H

#include "resource.h"
#include <wayland-server.h>
#include <type_traits>

namespace KWaylandServer
{

class Resource::Private
{
public:
    virtual ~Private();
    void create(ClientConnection *client, quint32 version, quint32 id);

    wl_resource *parentResource = nullptr;
    wl_resource *resource = nullptr;
    ClientConnection *client = nullptr;
    Global *global;

    template <typename ResourceDerived>
    static ResourceDerived *get(wl_resource *native) {
        static_assert(std::is_base_of<Resource, ResourceDerived>::value,
                      "ResourceDerived must be derived from Resource");
        if (!native) {
            return nullptr;
        }
        auto it = std::find_if(s_allResources.constBegin(), s_allResources.constEnd(),
            [native](Private *p) {
                return p->resource == native;
            }
        );
        if (it == s_allResources.constEnd()) {
            return nullptr;
        }
        return reinterpret_cast<ResourceDerived*>((*it)->q);
    }
    template <typename ResourceDerived>
    static ResourceDerived *get(quint32 id, const ClientConnection *c) {
        static_assert(std::is_base_of<Resource, ResourceDerived>::value,
                      "ResourceDerived must be derived from Resource");
        auto it = std::find_if(s_allResources.constBegin(), s_allResources.constEnd(),
            [id, c](Private *p) {
                return c == p->client && p->resource && wl_resource_get_id(p->resource) == id;
            }
        );
        if (it == s_allResources.constEnd()) {
            return nullptr;
        }
        return reinterpret_cast<ResourceDerived*>((*it)->q);
    }

protected:
    explicit Private(Resource *q, Global *g, wl_resource *parentResource, const wl_interface *interface, const void *implementation);

    template <typename Derived>
    static Derived *cast(wl_resource *r) {
        static_assert(std::is_base_of<Private, Derived>::value,
                      "Derived must be derived from Resource::Private");
        return r ? reinterpret_cast<Derived*>(wl_resource_get_user_data(r)) : nullptr;
    }
    static void unbind(wl_resource *resource);
    static void resourceDestroyedCallback(wl_client *client, wl_resource *resource);

    Resource *q;
    static QList<Private*> s_allResources;

private:
    const wl_interface *const m_interface;
    const void *const m_interfaceImplementation;
};

}

#endif
