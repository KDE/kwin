/*
    SPDX-FileCopyrightText: 2017 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "appmenu_interface.h"
#include "display.h"
#include "surface_interface.h"

#include <QtGlobal>

#include "qwayland-server-appmenu.h"

namespace KWaylandServer
{

static const quint32 s_version = 1;

class AppMenuManagerInterfacePrivate : public QtWaylandServer::org_kde_kwin_appmenu_manager
{
public:
    AppMenuManagerInterfacePrivate(AppMenuManagerInterface *q, Display *d);

    QVector<AppMenuInterface*> appmenus;
    AppMenuManagerInterface *q;

protected:
    void org_kde_kwin_appmenu_manager_create(Resource *resource, uint32_t id, wl_resource *surface) override;
};

void AppMenuManagerInterfacePrivate::org_kde_kwin_appmenu_manager_create(Resource *resource, uint32_t id, wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "Invalid  surface");
        return;
    }
    wl_resource *appmenu_resource = wl_resource_create(resource->client(), &org_kde_kwin_appmenu_interface, resource->version(), id);
    if (!appmenu_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }
    auto appmenu = new AppMenuInterface(s, appmenu_resource);

    appmenus.append(appmenu);
    QObject::connect(appmenu, &QObject::destroyed, q, [=]() {
        appmenus.removeOne(appmenu);
    });
    emit q->appMenuCreated(appmenu);
}

AppMenuManagerInterfacePrivate::AppMenuManagerInterfacePrivate(AppMenuManagerInterface *_q, Display *d)
    : QtWaylandServer::org_kde_kwin_appmenu_manager(*d, s_version)
    , q(_q)
{
}

class AppMenuInterfacePrivate : public QtWaylandServer::org_kde_kwin_appmenu
{
public:
    AppMenuInterfacePrivate(AppMenuInterface *q, SurfaceInterface *surface, wl_resource *resource);

    AppMenuInterface *q;
    QPointer<SurfaceInterface> surface;
    AppMenuInterface::InterfaceAddress address;
protected:
    void org_kde_kwin_appmenu_destroy_resource(Resource *resource) override;
    void org_kde_kwin_appmenu_set_address(Resource *resource, const QString &service_name, const QString &object_path) override;
    void org_kde_kwin_appmenu_release(Resource *resource) override;
};

AppMenuInterfacePrivate::AppMenuInterfacePrivate(AppMenuInterface *_q, SurfaceInterface *s, wl_resource *resource)
    : QtWaylandServer::org_kde_kwin_appmenu(resource)
    , q(_q)
    , surface(s)
{
}

void AppMenuInterfacePrivate::org_kde_kwin_appmenu_destroy_resource(QtWaylandServer::org_kde_kwin_appmenu::Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

void AppMenuInterfacePrivate::org_kde_kwin_appmenu_set_address(Resource *resource, const QString &service_name, const QString &object_path)
{
    Q_UNUSED(resource)
    if (address.serviceName == service_name &&
            address.objectPath == object_path) {
        return;
    }

    address.serviceName = service_name;
    address.objectPath = object_path;
    emit q->addressChanged(address);
}

void AppMenuInterfacePrivate::org_kde_kwin_appmenu_release(QtWaylandServer::org_kde_kwin_appmenu::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}


AppMenuManagerInterface::AppMenuManagerInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new AppMenuManagerInterfacePrivate(this, display))
{
}

AppMenuManagerInterface::~AppMenuManagerInterface()
{
}

AppMenuInterface* AppMenuManagerInterface::appMenuForSurface(SurfaceInterface *surface)
{
    for (AppMenuInterface* menu: d->appmenus) {
        if (menu->surface() == surface) {
            return menu;
        }
    }
    return nullptr;
}

AppMenuInterface::AppMenuInterface(SurfaceInterface *surface, wl_resource *resource)
    : QObject()
    , d(new AppMenuInterfacePrivate(this, surface, resource))
{
}

AppMenuInterface::~AppMenuInterface()
{}

AppMenuInterface::InterfaceAddress AppMenuInterface::address() const {
    return d->address;
}

SurfaceInterface* AppMenuInterface::surface() const {
    return d->surface.data();
}

}//namespace
