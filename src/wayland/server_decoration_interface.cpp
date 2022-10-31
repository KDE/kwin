/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "server_decoration_interface.h"
#include "display.h"
#include "surface_interface.h"
#include "utils/common.h"

#include <QVector>

#include <optional>

#include <qwayland-server-server-decoration.h>

namespace KWaylandServer
{
static const quint32 s_version = 1;

class ServerSideDecorationManagerInterfacePrivate : public QtWaylandServer::org_kde_kwin_server_decoration_manager
{
public:
    ServerSideDecorationManagerInterfacePrivate(ServerSideDecorationManagerInterface *_q, Display *display);
    void setDefaultMode(ServerSideDecorationManagerInterface::Mode mode);

    ServerSideDecorationManagerInterface::Mode defaultMode = ServerSideDecorationManagerInterface::Mode::None;
    ServerSideDecorationManagerInterface *q;

protected:
    void org_kde_kwin_server_decoration_manager_bind_resource(Resource *resource) override;
    void org_kde_kwin_server_decoration_manager_create(Resource *resource, uint32_t id, wl_resource *surface) override;
};

static uint32_t modeWayland(ServerSideDecorationManagerInterface::Mode mode)
{
    switch (mode) {
    case ServerSideDecorationManagerInterface::Mode::None:
        return ServerSideDecorationManagerInterfacePrivate::mode_None;
    case ServerSideDecorationManagerInterface::Mode::Client:
        return ServerSideDecorationManagerInterfacePrivate::mode_Client;
    case ServerSideDecorationManagerInterface::Mode::Server:
        return ServerSideDecorationManagerInterfacePrivate::mode_Server;
    default:
        Q_UNREACHABLE();
    }
}

void ServerSideDecorationManagerInterfacePrivate::org_kde_kwin_server_decoration_manager_create(Resource *resource, uint32_t id, wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "Invalid  surface");
        return;
    }

    wl_resource *decorationResource = wl_resource_create(resource->client(), &org_kde_kwin_server_decoration_interface, resource->version(), id);
    if (!decorationResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }
    auto decoration = new ServerSideDecorationInterface(q, s, decorationResource);
    decoration->setMode(defaultMode);
    Q_EMIT q->decorationCreated(decoration);
}

void ServerSideDecorationManagerInterfacePrivate::setDefaultMode(ServerSideDecorationManagerInterface::Mode mode)
{
    defaultMode = mode;
    const uint32_t wlMode = modeWayland(mode);

    const auto clientResources = resourceMap();
    for (Resource *resource : clientResources) {
        send_default_mode(resource->handle, wlMode);
    }
}

ServerSideDecorationManagerInterfacePrivate::ServerSideDecorationManagerInterfacePrivate(ServerSideDecorationManagerInterface *_q, Display *display)
    : QtWaylandServer::org_kde_kwin_server_decoration_manager(*display, s_version)
    , q(_q)
{
}

void ServerSideDecorationManagerInterfacePrivate::org_kde_kwin_server_decoration_manager_bind_resource(Resource *resource)
{
    send_default_mode(resource->handle, modeWayland(defaultMode));
}

ServerSideDecorationManagerInterface::ServerSideDecorationManagerInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new ServerSideDecorationManagerInterfacePrivate(this, display))
{
}

ServerSideDecorationManagerInterface::~ServerSideDecorationManagerInterface() = default;

void ServerSideDecorationManagerInterface::setDefaultMode(Mode mode)
{
    d->setDefaultMode(mode);
}

ServerSideDecorationManagerInterface::Mode ServerSideDecorationManagerInterface::defaultMode() const
{
    return d->defaultMode;
}

class ServerSideDecorationInterfacePrivate : public QtWaylandServer::org_kde_kwin_server_decoration
{
public:
    ServerSideDecorationInterfacePrivate(ServerSideDecorationManagerInterface *manager,
                                         ServerSideDecorationInterface *_q,
                                         SurfaceInterface *surface,
                                         wl_resource *resource);
    ~ServerSideDecorationInterfacePrivate();

    static ServerSideDecorationInterface *get(SurfaceInterface *surface);
    void setMode(ServerSideDecorationManagerInterface::Mode mode);

    ServerSideDecorationManagerInterface *manager;
    ServerSideDecorationManagerInterface::Mode mode = ServerSideDecorationManagerInterface::Mode::None;
    std::optional<ServerSideDecorationManagerInterface::Mode> preferredMode;
    SurfaceInterface *surface;

private:
    ServerSideDecorationInterface *q;
    static QVector<ServerSideDecorationInterfacePrivate *> s_all;

protected:
    void org_kde_kwin_server_decoration_destroy_resource(Resource *resource) override;
    void org_kde_kwin_server_decoration_release(Resource *resource) override;
    void org_kde_kwin_server_decoration_request_mode(Resource *resource, uint32_t mode) override;
};

QVector<ServerSideDecorationInterfacePrivate *> ServerSideDecorationInterfacePrivate::s_all;

void ServerSideDecorationInterfacePrivate::org_kde_kwin_server_decoration_request_mode(Resource *resource, uint32_t mode)
{
    ServerSideDecorationManagerInterface::Mode m = ServerSideDecorationManagerInterface::Mode::None;
    switch (mode) {
    case ServerSideDecorationManagerInterfacePrivate::mode_None:
        m = ServerSideDecorationManagerInterface::Mode::None;
        break;
    case ServerSideDecorationManagerInterfacePrivate::mode_Client:
        m = ServerSideDecorationManagerInterface::Mode::Client;
        break;
    case ServerSideDecorationManagerInterfacePrivate::mode_Server:
        m = ServerSideDecorationManagerInterface::Mode::Server;
        break;
    default:
        // invalid mode
        qCWarning(KWIN_CORE) << "Invalid mode:" << mode;
        return;
    }
    preferredMode = m;
    Q_EMIT q->preferredModeChanged();
}

void ServerSideDecorationInterfacePrivate::org_kde_kwin_server_decoration_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ServerSideDecorationInterfacePrivate::org_kde_kwin_server_decoration_destroy_resource(Resource *resource)
{
    delete q;
}

ServerSideDecorationInterface *ServerSideDecorationInterfacePrivate::get(SurfaceInterface *surface)
{
    for (ServerSideDecorationInterfacePrivate *decoration : std::as_const(s_all)) {
        if (decoration->surface == surface) {
            return decoration->q;
        }
    }
    return nullptr;
}

ServerSideDecorationInterfacePrivate::ServerSideDecorationInterfacePrivate(ServerSideDecorationManagerInterface *manager,
                                                                           ServerSideDecorationInterface *_q,
                                                                           SurfaceInterface *surface,
                                                                           wl_resource *resource)
    : QtWaylandServer::org_kde_kwin_server_decoration(resource)
    , manager(manager)
    , surface(surface)
    , q(_q)
{
    s_all << this;
}

ServerSideDecorationInterfacePrivate::~ServerSideDecorationInterfacePrivate()
{
    s_all.removeAll(this);
}

void ServerSideDecorationInterfacePrivate::setMode(ServerSideDecorationManagerInterface::Mode mode)
{
    this->mode = mode;
    send_mode(modeWayland(mode));
}

ServerSideDecorationInterface::ServerSideDecorationInterface(ServerSideDecorationManagerInterface *manager, SurfaceInterface *surface, wl_resource *resource)
    : QObject()
    , d(new ServerSideDecorationInterfacePrivate(manager, this, surface, resource))
{
}

ServerSideDecorationInterface::~ServerSideDecorationInterface() = default;

void ServerSideDecorationInterface::setMode(ServerSideDecorationManagerInterface::Mode mode)
{
    d->setMode(mode);
}

ServerSideDecorationManagerInterface::Mode ServerSideDecorationInterface::mode() const
{
    return d->mode;
}

ServerSideDecorationManagerInterface::Mode ServerSideDecorationInterface::preferredMode() const
{
    return d->preferredMode.value_or(d->manager->defaultMode());
}

SurfaceInterface *ServerSideDecorationInterface::surface() const
{
    return d->surface;
}

ServerSideDecorationInterface *ServerSideDecorationInterface::get(SurfaceInterface *surface)
{
    return ServerSideDecorationInterfacePrivate::get(surface);
}

}
