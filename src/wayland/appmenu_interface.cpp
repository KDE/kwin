/****************************************************************************
Copyright 2017  David Edmundson <kde@davidedmundson.co.uk>

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
#include "appmenu_interface.h"
#include "display.h"
#include "surface_interface.h"
#include "global_p.h"
#include "resource_p.h"
#include "logging_p.h"

#include <QtGlobal>

#include <wayland-appmenu-server-protocol.h>

namespace KWayland
{
namespace Server
{
class AppMenuManagerInterface::Private : public Global::Private
{
public:
    Private(AppMenuManagerInterface *q, Display *d);

    QVector<AppMenuInterface*> appmenus;
private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    static void createCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * surface);

    AppMenuManagerInterface *q;
    static const struct org_kde_kwin_appmenu_manager_interface s_interface;
    static const quint32 s_version;
};

const quint32 AppMenuManagerInterface::Private::s_version = 1;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct org_kde_kwin_appmenu_manager_interface AppMenuManagerInterface::Private::s_interface = {
    createCallback
};
#endif

void AppMenuManagerInterface::Private::createCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * surface)
{
    auto p = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    Q_ASSERT(p);

    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        // TODO: send error?
        qCWarning(KWAYLAND_SERVER) << "ServerSideDecorationInterface requested for non existing SurfaceInterface";
        return;
    }
    auto appmenu = new AppMenuInterface(p->q, s, resource);
    appmenu->create(p->display->getConnection(client), wl_resource_get_version(resource), id);
    if (!appmenu->resource()) {
        wl_resource_post_no_memory(resource);
        delete appmenu;
        return;
    }
    p->appmenus.append(appmenu);
    QObject::connect(appmenu, &QObject::destroyed, p->q, [=]() {
        p->appmenus.removeOne(appmenu);
    });
    emit p->q->appMenuCreated(appmenu);
}

AppMenuManagerInterface::Private::Private(AppMenuManagerInterface *q, Display *d)
    : Global::Private(d, &org_kde_kwin_appmenu_manager_interface, s_version)
    , q(q)
{
}

void AppMenuManagerInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&org_kde_kwin_appmenu_manager_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
}

void AppMenuManagerInterface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
}

class AppMenuInterface::Private : public Resource::Private
{
public:
    Private(AppMenuInterface *q, AppMenuManagerInterface *c, SurfaceInterface *surface, wl_resource *parentResource);
    ~Private();


    SurfaceInterface *surface;
    InterfaceAddress address;
private:
    static void setAddressCallback(wl_client *client, wl_resource *resource, const char * service_name, const char * object_path);

    AppMenuInterface *q_func() {
        return reinterpret_cast<AppMenuInterface *>(q);
    }
    static AppMenuInterface *get(SurfaceInterface *s);
    static const struct org_kde_kwin_appmenu_interface s_interface;
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct org_kde_kwin_appmenu_interface AppMenuInterface::Private::s_interface = {
    setAddressCallback,
    resourceDestroyedCallback
};
#endif

void AppMenuInterface::Private::setAddressCallback(wl_client *client, wl_resource *resource, const char * service_name, const char * object_path)
{
    Q_UNUSED(client);
    auto p = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    Q_ASSERT(p);

    if (p->address.serviceName == QLatin1String(service_name) &&
        p->address.objectPath == QLatin1String(object_path)) {
        return;
    }
    p->address.serviceName = QString::fromLatin1(service_name);
    p->address.objectPath = QString::fromLatin1(object_path);
    emit p->q_func()->addressChanged(p->address);
}

AppMenuInterface::Private::Private(AppMenuInterface *q, AppMenuManagerInterface *c, SurfaceInterface *s, wl_resource *parentResource)
    : Resource::Private(q, c, parentResource, &org_kde_kwin_appmenu_interface, &s_interface),
    surface(s)
{
}

AppMenuInterface::Private::~Private()
{
    if (resource) {
        wl_resource_destroy(resource);
        resource = nullptr;
    }
}

AppMenuManagerInterface::AppMenuManagerInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

AppMenuManagerInterface::~AppMenuManagerInterface()
{
}

AppMenuManagerInterface::Private *AppMenuManagerInterface::d_func() const
{
    return reinterpret_cast<AppMenuManagerInterface::Private*>(d.data());
}

AppMenuInterface* AppMenuManagerInterface::appMenuForSurface(SurfaceInterface *surface)
{
    Q_D();
    for (AppMenuInterface* menu: d->appmenus) {
        if (menu->surface() == surface) {
            return menu;
        }
    }
    return nullptr;
}

AppMenuInterface::AppMenuInterface(AppMenuManagerInterface *parent, SurfaceInterface *s, wl_resource *parentResource):
    Resource(new Private(this, parent, s, parentResource))
{
}

AppMenuInterface::Private *AppMenuInterface::d_func() const
{
    return reinterpret_cast<AppMenuInterface::Private*>(d.data());
}

AppMenuInterface::~AppMenuInterface()
{}

AppMenuInterface::InterfaceAddress AppMenuInterface::address() const {
    Q_D();
    return d->address;
}

SurfaceInterface* AppMenuInterface::surface() const {
    Q_D();
    return d->surface;
}

}//namespace
}

