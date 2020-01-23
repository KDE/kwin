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
#include "server_decoration_palette_interface.h"
#include "display.h"
#include "surface_interface.h"
#include "global_p.h"
#include "resource_p.h"
#include "logging.h"

#include <QtGlobal>

#include <wayland-server_decoration_palette-server-protocol.h>

namespace KWayland
{
namespace Server
{
class ServerSideDecorationPaletteManagerInterface::Private : public Global::Private
{
public:
    Private(ServerSideDecorationPaletteManagerInterface *q, Display *d);

    QVector<ServerSideDecorationPaletteInterface*> palettes;
private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    static void createCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * surface);

    ServerSideDecorationPaletteManagerInterface *q;
    static const struct org_kde_kwin_server_decoration_palette_manager_interface s_interface;
    static const quint32 s_version;
};

const quint32 ServerSideDecorationPaletteManagerInterface::Private::s_version = 1;

#ifndef K_DOXYGEN
const struct org_kde_kwin_server_decoration_palette_manager_interface ServerSideDecorationPaletteManagerInterface::Private::s_interface = {
    createCallback
};
#endif

void ServerSideDecorationPaletteManagerInterface::Private::createCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * surface)
{
    auto p = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    Q_ASSERT(p);

    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        // TODO: send error?
        qCWarning(KWAYLAND_SERVER) << "ServerSideDecorationPaletteInterface requested for non existing SurfaceInterface";
        return;
    }
    auto palette = new ServerSideDecorationPaletteInterface(p->q, s, resource);
    palette->create(p->display->getConnection(client), wl_resource_get_version(resource), id);
    if (!palette->resource()) {
        wl_resource_post_no_memory(resource);
        delete palette;
        return;
    }
    p->palettes.append(palette);
    QObject::connect(palette, &QObject::destroyed, p->q, [=]() {
        p->palettes.removeOne(palette);
    });
    emit p->q->paletteCreated(palette);
}

ServerSideDecorationPaletteManagerInterface::Private::Private(ServerSideDecorationPaletteManagerInterface *q, Display *d)
    : Global::Private(d, &org_kde_kwin_server_decoration_palette_manager_interface, s_version)
    , q(q)
{
}

void ServerSideDecorationPaletteManagerInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&org_kde_kwin_server_decoration_palette_manager_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
}

void ServerSideDecorationPaletteManagerInterface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
}

class ServerSideDecorationPaletteInterface::Private : public Resource::Private
{
public:
    Private(ServerSideDecorationPaletteInterface *q, ServerSideDecorationPaletteManagerInterface *c, SurfaceInterface *surface, wl_resource *parentResource);
    ~Private();


    SurfaceInterface *surface;
    QString palette;
private:
    static void setPaletteCallback(wl_client *client, wl_resource *resource, const char * palette);

    ServerSideDecorationPaletteInterface *q_func() {
        return reinterpret_cast<ServerSideDecorationPaletteInterface *>(q);
    }
    static ServerSideDecorationPaletteInterface *get(SurfaceInterface *s);
    static const struct org_kde_kwin_server_decoration_palette_interface s_interface;
};

#ifndef K_DOXYGEN
const struct org_kde_kwin_server_decoration_palette_interface ServerSideDecorationPaletteInterface::Private::s_interface = {
    setPaletteCallback,
    resourceDestroyedCallback
};
#endif

void ServerSideDecorationPaletteInterface::Private::setPaletteCallback(wl_client *client, wl_resource *resource, const char * palette)
{
    Q_UNUSED(client);
    auto p = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    Q_ASSERT(p);

    if (p->palette == QLatin1String(palette)) {
        return;
    }
    p->palette = QString::fromUtf8(palette);
    emit p->q_func()->paletteChanged(p->palette);
}

ServerSideDecorationPaletteInterface::Private::Private(ServerSideDecorationPaletteInterface *q, ServerSideDecorationPaletteManagerInterface *c, SurfaceInterface *s, wl_resource *parentResource)
    : Resource::Private(q, c, parentResource, &org_kde_kwin_server_decoration_palette_interface, &s_interface),
    surface(s)
{
}

ServerSideDecorationPaletteInterface::Private::~Private()
{
    if (resource) {
        wl_resource_destroy(resource);
        resource = nullptr;
    }
}

ServerSideDecorationPaletteManagerInterface::ServerSideDecorationPaletteManagerInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

ServerSideDecorationPaletteManagerInterface::~ServerSideDecorationPaletteManagerInterface()
{
}

ServerSideDecorationPaletteManagerInterface::Private *ServerSideDecorationPaletteManagerInterface::d_func() const
{
    return reinterpret_cast<ServerSideDecorationPaletteManagerInterface::Private*>(d.data());
}

ServerSideDecorationPaletteInterface* ServerSideDecorationPaletteManagerInterface::paletteForSurface(SurfaceInterface *surface)
{
    Q_D();
    for (ServerSideDecorationPaletteInterface* menu: d->palettes) {
        if (menu->surface() == surface) {
            return menu;
        }
    }
    return nullptr;
}

ServerSideDecorationPaletteInterface::ServerSideDecorationPaletteInterface(ServerSideDecorationPaletteManagerInterface *parent, SurfaceInterface *s, wl_resource *parentResource):
    Resource(new Private(this, parent, s, parentResource))
{
}

ServerSideDecorationPaletteInterface::Private *ServerSideDecorationPaletteInterface::d_func() const
{
    return reinterpret_cast<ServerSideDecorationPaletteInterface::Private*>(d.data());
}

ServerSideDecorationPaletteInterface::~ServerSideDecorationPaletteInterface()
{}

QString ServerSideDecorationPaletteInterface::palette() const
{
    Q_D();
    return d->palette;
}

SurfaceInterface* ServerSideDecorationPaletteInterface::surface() const
{
    Q_D();
    return d->surface;
}

}//namespace
}

