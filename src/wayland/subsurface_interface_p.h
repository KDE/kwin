/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_SUBSURFACE_INTERFACE_P_H
#define WAYLAND_SERVER_SUBSURFACE_INTERFACE_P_H

#include "subcompositor_interface.h"
#include "resource_p.h"
// Qt
#include <QPoint>
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class SubSurfaceInterface::Private : public Resource::Private
{
public:
    Private(SubSurfaceInterface *q, SubCompositorInterface *compositor, wl_resource *parentResource);
    ~Private();

    using Resource::Private::create;
    void create(ClientConnection *client, quint32 version, quint32 id, SurfaceInterface *surface, SurfaceInterface *parent);
    void commit();

    QPoint pos = QPoint(0, 0);
    QPoint scheduledPos = QPoint();
    bool scheduledPosChange = false;
    Mode mode = Mode::Synchronized;
    QPointer<SurfaceInterface> surface;
    QPointer<SurfaceInterface> parent;

private:
    SubSurfaceInterface *q_func() {
        return reinterpret_cast<SubSurfaceInterface *>(q);
    }
    void setMode(Mode mode);
    void setPosition(const QPoint &pos);
    void placeAbove(SurfaceInterface *sibling);
    void placeBelow(SurfaceInterface *sibling);

    static void setPositionCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y);
    static void placeAboveCallback(wl_client *client, wl_resource *resource, wl_resource *sibling);
    static void placeBelowCallback(wl_client *client, wl_resource *resource, wl_resource *sibling);
    static void setSyncCallback(wl_client *client, wl_resource *resource);
    static void setDeSyncCallback(wl_client *client, wl_resource *resource);

    static const struct wl_subsurface_interface s_interface;
};

}
}

#endif
