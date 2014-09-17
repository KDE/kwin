/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_WAYLAND_SERVER_COMPOSITOR_INTERFACE_H
#define KWIN_WAYLAND_SERVER_COMPOSITOR_INTERFACE_H

#include "surface_interface.h"

#include <QObject>

#include <wayland-server.h>

#include <kwaylandserver_export.h>

namespace KWin
{
namespace WaylandServer
{

class Display;
class SurfaceInterface;

class KWAYLANDSERVER_EXPORT CompositorInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~CompositorInterface();

    void create();
    void destroy();
    bool isValid() const {
        return m_compositor != nullptr;
    }

Q_SIGNALS:
    void surfaceCreated(KWin::WaylandServer::SurfaceInterface*);

private:
    explicit CompositorInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    static void bind(wl_client *client, void *data, uint32_t version, uint32_t id);
    static void unbind(wl_resource *resource);
    static void createSurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static void createRegionCallback(wl_client *client, wl_resource *resource, uint32_t id);
    void bind(wl_client *client, uint32_t version, uint32_t id);
    void createSurface(wl_client *client, wl_resource *resource, uint32_t id);
    void createRegion(wl_client *client, wl_resource *resource, uint32_t id);
    static CompositorInterface *cast(wl_resource *r) {
        return reinterpret_cast<CompositorInterface*>(wl_resource_get_user_data(r));
    }
    static const struct wl_compositor_interface s_interface;
    Display *m_display;
    wl_global *m_compositor;
};

}
}

#endif
