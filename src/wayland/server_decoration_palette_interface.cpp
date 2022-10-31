/*
    SPDX-FileCopyrightText: 2017 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "server_decoration_palette_interface.h"
#include "display.h"
#include "surface_interface.h"
#include "utils/common.h"

#include <QtGlobal>

#include <qwayland-server-server-decoration-palette.h>

namespace KWaylandServer
{
static const quint32 s_version = 1;

class ServerSideDecorationPaletteManagerInterfacePrivate : public QtWaylandServer::org_kde_kwin_server_decoration_palette_manager
{
public:
    ServerSideDecorationPaletteManagerInterfacePrivate(ServerSideDecorationPaletteManagerInterface *q, Display *display);

    QVector<ServerSideDecorationPaletteInterface *> palettes;
    ServerSideDecorationPaletteManagerInterface *q;

protected:
    void org_kde_kwin_server_decoration_palette_manager_create(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
};

void ServerSideDecorationPaletteManagerInterfacePrivate::org_kde_kwin_server_decoration_palette_manager_create(Resource *resource,
                                                                                                               uint32_t id,
                                                                                                               wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "invalid surface");
        qCWarning(KWIN_CORE) << "ServerSideDecorationPaletteInterface requested for non existing SurfaceInterface";
        return;
    }

    wl_resource *palette_resource = wl_resource_create(resource->client(), &org_kde_kwin_server_decoration_palette_interface, resource->version(), id);
    if (!palette_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }
    auto palette = new ServerSideDecorationPaletteInterface(s, palette_resource);

    palettes.append(palette);
    QObject::connect(palette, &QObject::destroyed, q, [this, palette]() {
        palettes.removeOne(palette);
    });
    Q_EMIT q->paletteCreated(palette);
}

ServerSideDecorationPaletteManagerInterfacePrivate::ServerSideDecorationPaletteManagerInterfacePrivate(ServerSideDecorationPaletteManagerInterface *_q,
                                                                                                       Display *display)
    : QtWaylandServer::org_kde_kwin_server_decoration_palette_manager(*display, s_version)
    , q(_q)
{
}

ServerSideDecorationPaletteManagerInterface::ServerSideDecorationPaletteManagerInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new ServerSideDecorationPaletteManagerInterfacePrivate(this, display))
{
}

ServerSideDecorationPaletteManagerInterface::~ServerSideDecorationPaletteManagerInterface() = default;

ServerSideDecorationPaletteInterface *ServerSideDecorationPaletteManagerInterface::paletteForSurface(SurfaceInterface *surface)
{
    for (ServerSideDecorationPaletteInterface *menu : std::as_const(d->palettes)) {
        if (menu->surface() == surface) {
            return menu;
        }
    }
    return nullptr;
}

class ServerSideDecorationPaletteInterfacePrivate : public QtWaylandServer::org_kde_kwin_server_decoration_palette
{
public:
    ServerSideDecorationPaletteInterfacePrivate(ServerSideDecorationPaletteInterface *_q, SurfaceInterface *surface, wl_resource *resource);

    SurfaceInterface *surface;
    QString palette;
    ServerSideDecorationPaletteInterface *q;

protected:
    void org_kde_kwin_server_decoration_palette_destroy_resource(Resource *resource) override;
    void org_kde_kwin_server_decoration_palette_set_palette(Resource *resource, const QString &palette) override;
    void org_kde_kwin_server_decoration_palette_release(Resource *resource) override;
};

void ServerSideDecorationPaletteInterfacePrivate::org_kde_kwin_server_decoration_palette_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ServerSideDecorationPaletteInterfacePrivate::org_kde_kwin_server_decoration_palette_set_palette(Resource *resource, const QString &palette)
{
    if (this->palette == palette) {
        return;
    }
    this->palette = palette;
    Q_EMIT q->paletteChanged(this->palette);
}

void ServerSideDecorationPaletteInterfacePrivate::org_kde_kwin_server_decoration_palette_destroy_resource(Resource *resource)
{
    delete q;
}

ServerSideDecorationPaletteInterfacePrivate::ServerSideDecorationPaletteInterfacePrivate(ServerSideDecorationPaletteInterface *_q,
                                                                                         SurfaceInterface *surface,
                                                                                         wl_resource *resource)
    : QtWaylandServer::org_kde_kwin_server_decoration_palette(resource)
    , surface(surface)
    , q(_q)
{
}

ServerSideDecorationPaletteInterface::ServerSideDecorationPaletteInterface(SurfaceInterface *surface, wl_resource *resource)
    : QObject()
    , d(new ServerSideDecorationPaletteInterfacePrivate(this, surface, resource))
{
}

ServerSideDecorationPaletteInterface::~ServerSideDecorationPaletteInterface() = default;

QString ServerSideDecorationPaletteInterface::palette() const
{
    return d->palette;
}

SurfaceInterface *ServerSideDecorationPaletteInterface::surface() const
{
    return d->surface;
}

} // namespace
