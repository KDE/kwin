/****************************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

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
#include "server_decoration_interface.h"
#include "display.h"
#include "global_p.h"
#include "logging.h"
#include "resource_p.h"
#include "surface_interface.h"

#include <QVector>

#include <wayland-server_decoration-server-protocol.h>

namespace KWayland
{
namespace Server
{

class ServerSideDecorationManagerInterface::Private : public Global::Private
{
public:
    Private(ServerSideDecorationManagerInterface *q, Display *d);

    Mode defaultMode = Mode::None;

    QVector<wl_resource*> resources;

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    static void createCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * surface);
    void create(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * surface);

    ServerSideDecorationManagerInterface *q;
    static const struct org_kde_kwin_server_decoration_manager_interface s_interface;
    static const quint32 s_version;
};

const quint32 ServerSideDecorationManagerInterface::Private::s_version = 1;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct org_kde_kwin_server_decoration_manager_interface ServerSideDecorationManagerInterface::Private::s_interface = {
    createCallback
};
#endif

void ServerSideDecorationManagerInterface::Private::createCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface)
{
    reinterpret_cast<Private*>(wl_resource_get_user_data(resource))->create(client, resource, id, surface);
}

void ServerSideDecorationManagerInterface::Private::create(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        // TODO: send error?
        qCWarning(KWAYLAND_SERVER) << "ServerSideDecorationInterface requested for non existing SurfaceInterface";
        return;
    }
    ServerSideDecorationInterface *decoration = new ServerSideDecorationInterface(q, s, resource);
    decoration->create(display->getConnection(client), wl_resource_get_version(resource), id);
    if (!decoration->resource()) {
        wl_resource_post_no_memory(resource);
        delete decoration;
        return;
    }
    decoration->setMode(defaultMode);
    emit q->decorationCreated(decoration);
}

ServerSideDecorationManagerInterface::Mode ServerSideDecorationManagerInterface::defaultMode() const
{
    return d_func()->defaultMode;
}

ServerSideDecorationManagerInterface::Private::Private(ServerSideDecorationManagerInterface *q, Display *d)
    : Global::Private(d, &org_kde_kwin_server_decoration_manager_interface, s_version)
    , q(q)
{
}

namespace {
static uint32_t modeWayland(ServerSideDecorationManagerInterface::Mode mode)
{
    switch (mode) {
    case ServerSideDecorationManagerInterface::Mode::None:
        return ORG_KDE_KWIN_SERVER_DECORATION_MODE_NONE;
        break;
    case ServerSideDecorationManagerInterface::Mode::Client:
        return ORG_KDE_KWIN_SERVER_DECORATION_MODE_CLIENT;
        break;
    case ServerSideDecorationManagerInterface::Mode::Server:
        return ORG_KDE_KWIN_SERVER_DECORATION_MODE_SERVER;
        break;
    default:
        Q_UNREACHABLE();
    }
}
}

void ServerSideDecorationManagerInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&org_kde_kwin_server_decoration_manager_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);

    resources << resource;

    org_kde_kwin_server_decoration_manager_send_default_mode(resource, modeWayland(defaultMode));
    c->flush();
}

void ServerSideDecorationManagerInterface::Private::unbind(wl_resource *resource)
{
    cast(resource)->resources.removeAll(resource);
}

ServerSideDecorationManagerInterface::ServerSideDecorationManagerInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

ServerSideDecorationManagerInterface::~ServerSideDecorationManagerInterface() = default;

ServerSideDecorationManagerInterface::Private *ServerSideDecorationManagerInterface::d_func() const
{
    return reinterpret_cast<ServerSideDecorationManagerInterface::Private*>(d.data());
}

void ServerSideDecorationManagerInterface::setDefaultMode(Mode mode)
{
    Q_D();
    d->defaultMode = mode;
    const uint32_t wlMode = modeWayland(mode);
    for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); it++) {
        org_kde_kwin_server_decoration_manager_send_default_mode(*it, wlMode);
    }
}

class ServerSideDecorationInterface::Private : public Resource::Private
{
public:
    Private(ServerSideDecorationInterface *q, ServerSideDecorationManagerInterface *c, SurfaceInterface *surface,  wl_resource *parentResource);
    ~Private();

    ServerSideDecorationManagerInterface::Mode mode = ServerSideDecorationManagerInterface::Mode::None;
    SurfaceInterface *surface;

    static ServerSideDecorationInterface *get(SurfaceInterface *s);

private:
    static void requestModeCallback(wl_client *client, wl_resource *resource, uint32_t mode);

    ServerSideDecorationInterface *q_func() {
        return reinterpret_cast<ServerSideDecorationInterface *>(q);
    }

    static const struct org_kde_kwin_server_decoration_interface s_interface;
    static QVector<Private*> s_all;
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct org_kde_kwin_server_decoration_interface ServerSideDecorationInterface::Private::s_interface = {
    resourceDestroyedCallback,
    requestModeCallback
};
QVector<ServerSideDecorationInterface::Private*> ServerSideDecorationInterface::Private::s_all;
#endif

void ServerSideDecorationInterface::Private::requestModeCallback(wl_client *client, wl_resource *resource, uint32_t mode)
{
    Q_UNUSED(client)
    ServerSideDecorationManagerInterface::Mode m = ServerSideDecorationManagerInterface::Mode::None;
    switch (mode) {
    case ORG_KDE_KWIN_SERVER_DECORATION_MODE_NONE:
        m = ServerSideDecorationManagerInterface::Mode::None;
        break;
    case ORG_KDE_KWIN_SERVER_DECORATION_MODE_CLIENT:
        m = ServerSideDecorationManagerInterface::Mode::Client;
        break;
    case ORG_KDE_KWIN_SERVER_DECORATION_MODE_SERVER:
        m = ServerSideDecorationManagerInterface::Mode::Server;
        break;
    default:
        // invalid mode
        qCWarning(KWAYLAND_SERVER) << "Invalid mode:" << mode;
        return;
    }
    emit cast<Private>(resource)->q_func()->modeRequested(m);
}

ServerSideDecorationInterface *ServerSideDecorationInterface::Private::get(SurfaceInterface *s)
{
    auto it = std::find_if(s_all.constBegin(), s_all.constEnd(), [s] (Private *p) { return p->surface == s; });
    if (it == s_all.constEnd()) {
        return nullptr;
    }
    return (*it)->q_func();
}

ServerSideDecorationInterface::Private::Private(ServerSideDecorationInterface *q, ServerSideDecorationManagerInterface *c, SurfaceInterface *surface, wl_resource *parentResource)
    : Resource::Private(q, c, parentResource, &org_kde_kwin_server_decoration_interface, &s_interface)
    , surface(surface)
{
    s_all << this;
}

ServerSideDecorationInterface::Private::~Private()
{
    s_all.removeAll(this);
}

ServerSideDecorationInterface::ServerSideDecorationInterface(ServerSideDecorationManagerInterface *parent, SurfaceInterface *surface, wl_resource *parentResource)
    : Resource(new Private(this, parent, surface, parentResource))
{
}

ServerSideDecorationInterface::~ServerSideDecorationInterface() = default;

void ServerSideDecorationInterface::setMode(ServerSideDecorationManagerInterface::Mode mode)
{
    Q_D();
    d->mode = mode;
    org_kde_kwin_server_decoration_send_mode(resource(), modeWayland(mode));
    client()->flush();
}

ServerSideDecorationManagerInterface::Mode ServerSideDecorationInterface::mode() const
{
    Q_D();
    return d->mode;
}

SurfaceInterface *ServerSideDecorationInterface::surface() const
{
    Q_D();
    return d->surface;
}

ServerSideDecorationInterface::Private *ServerSideDecorationInterface::d_func() const
{
    return reinterpret_cast<ServerSideDecorationInterface::Private*>(d.data());
}

ServerSideDecorationInterface *ServerSideDecorationInterface::get(SurfaceInterface *s)
{
    return Private::get(s);
}

}
}
