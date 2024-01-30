/*
    SPDX-FileCopyrightText: 2017 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "appmenu.h"
#include "display.h"
#include "surface.h"

#include <QPointer>
#include <QtGlobal>

#include "qwayland-server-xdg-dbus-annotation-v1.h"
#include "wayland_server.h"

namespace KWin
{
static const quint32 s_version = 1;
static const QString s_menuDbusInterface = QStringLiteral("com.canonical.dbusmenu");

class AppMenuManagerInterfacePrivate : public QtWaylandServer::xdg_dbus_annotation_manager_v1
{
public:
    AppMenuManagerInterfacePrivate(AppMenuManagerInterface *q, Display *d);

    QList<AppMenuInterface *> appmenus;
    AppMenuManagerInterface *q;

protected:
    void xdg_dbus_annotation_manager_v1_create_client(Resource *resource, const QString &interface, uint32_t id) override;
    void xdg_dbus_annotation_manager_v1_create_surface(Resource *resource, const QString &interface, uint32_t id, struct ::wl_resource *toplevel) override;
};

void AppMenuManagerInterfacePrivate::xdg_dbus_annotation_manager_v1_create_surface(
    Resource *resource, const QString &interface, uint32_t id, struct ::wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "Invalid surface");
        return;
    }
    wl_resource *appmenu_resource = wl_resource_create(resource->client(), QtWaylandServer::xdg_dbus_annotation_v1::interface(), resource->version(), id);
    if (!appmenu_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }
    auto appmenu = new SurfaceAppMenuInterface(s, appmenu_resource);

    appmenus.append(appmenu);
    QObject::connect(appmenu, &QObject::destroyed, q, [=, this]() {
        appmenus.removeOne(appmenu);
    });
    Q_EMIT q->appMenuCreated(appmenu);
    Q_EMIT q->surfaceAppMenuCreated(appmenu);
}

void AppMenuManagerInterfacePrivate::xdg_dbus_annotation_manager_v1_create_client(
    Resource *resource, const QString &interface, uint32_t id)
{
    auto connection = waylandServer()->display()->getConnection(resource->client());
    wl_resource *appmenu_resource = wl_resource_create(resource->client(), QtWaylandServer::xdg_dbus_annotation_v1::interface(), resource->version(), id);
    if (!appmenu_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }
    auto appmenu = new ClientAppMenuInterface(connection, appmenu_resource);

    appmenus.append(appmenu);
    QObject::connect(appmenu, &QObject::destroyed, q, [=, this]() {
        appmenus.removeOne(appmenu);
    });
    Q_EMIT q->appMenuCreated(appmenu);
    Q_EMIT q->clientAppMenuCreated(appmenu);
}

AppMenuManagerInterfacePrivate::AppMenuManagerInterfacePrivate(AppMenuManagerInterface *_q, Display *d)
    : QtWaylandServer::xdg_dbus_annotation_manager_v1(*d, s_version)
    , q(_q)
{
}

class AppMenuInterfacePrivate : public QtWaylandServer::xdg_dbus_annotation_v1
{
public:
    AppMenuInterfacePrivate(AppMenuInterface *q, wl_resource *resource);

    AppMenuInterface *q;
    AppMenuInterface::InterfaceAddress address;

protected:
    void xdg_dbus_annotation_v1_destroy_resource(Resource *resource) override;
    void xdg_dbus_annotation_v1_destroy(Resource *resource) override;
    void xdg_dbus_annotation_v1_set_address(Resource *resource, const QString &service_name, const QString &object_path) override;
};

AppMenuInterfacePrivate::AppMenuInterfacePrivate(AppMenuInterface *_q, wl_resource *resource)
    : QtWaylandServer::xdg_dbus_annotation_v1(resource)
    , q(_q)
{
}

void AppMenuInterfacePrivate::xdg_dbus_annotation_v1_destroy_resource(QtWaylandServer::xdg_dbus_annotation_v1::Resource *resource)
{
    delete q;
}

void AppMenuInterfacePrivate::xdg_dbus_annotation_v1_set_address(Resource *resource, const QString &service_name, const QString &object_path)
{
    if (address.serviceName == service_name && address.objectPath == object_path) {
        return;
    }

    address.serviceName = service_name;
    address.objectPath = object_path;
    Q_EMIT q->addressChanged(address);
}

void AppMenuInterfacePrivate::xdg_dbus_annotation_v1_destroy(QtWaylandServer::xdg_dbus_annotation_v1::Resource *resource)
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

SurfaceAppMenuInterface *AppMenuManagerInterface::appMenuForSurface(SurfaceInterface *surface)
{
    for (AppMenuInterface *menu : d->appmenus) {
        auto it = qobject_cast<SurfaceAppMenuInterface *>(menu);
        if (it && it->surface() == surface) {
            return it;
        }
    }
    return nullptr;
}

AppMenuInterface::AppMenuInterface(wl_resource *resource)
    : QObject()
    , d(new AppMenuInterfacePrivate(this, resource))
{
}

AppMenuInterface::~AppMenuInterface()
{
}

AppMenuInterface::InterfaceAddress AppMenuInterface::address() const
{
    return d->address;
}

SurfaceAppMenuInterface::SurfaceAppMenuInterface(SurfaceInterface *surface, wl_resource *resource)
    : AppMenuInterface(resource)
    , m_surface(surface)
{
}

SurfaceAppMenuInterface::~SurfaceAppMenuInterface()
{
}

SurfaceInterface *SurfaceAppMenuInterface::surface() const
{
    return m_surface;
}

ClientAppMenuInterface::ClientAppMenuInterface(ClientConnection *client, wl_resource *resource)
    : AppMenuInterface(resource)
    , m_client(client)
{
}

ClientAppMenuInterface::~ClientAppMenuInterface()
{
}

ClientConnection *ClientAppMenuInterface::client() const
{
    return m_client;
}

} // namespace
