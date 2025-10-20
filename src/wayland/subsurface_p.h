/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "subcompositor.h"

#include <QPoint>
#include <QPointer>

#include "qwayland-server-wayland.h"

namespace KWin
{
class SubCompositorInterfacePrivate : public QtWaylandServer::wl_subcompositor
{
public:
    SubCompositorInterfacePrivate(Display *display, SubCompositorInterface *q);

    SubCompositorInterface *q;

protected:
    void subcompositor_destroy(Resource *resource) override;
    void subcompositor_get_subsurface(Resource *resource, uint32_t id, struct ::wl_resource *surface_resource, struct ::wl_resource *parent_resource) override;
};

class SubSurfaceInterfacePrivate : public QtWaylandServer::wl_subsurface
{
public:
    static SubSurfaceInterfacePrivate *get(SubSurfaceInterface *subsurface);

    SubSurfaceInterfacePrivate(SubSurfaceInterface *q, SurfaceInterface *surface, SurfaceInterface *parent, ::wl_resource *resource);

    SubSurfaceInterface *q;
    QPointF position = QPointF(0, 0);
    SubSurfaceInterface::Mode mode = SubSurfaceInterface::Mode::Synchronized;
    QPointer<SurfaceInterface> surface;
    QPointer<SurfaceInterface> parent;

protected:
    void subsurface_destroy_resource(Resource *resource) override;
    void subsurface_destroy(Resource *resource) override;
    void subsurface_set_position(Resource *resource, int32_t x, int32_t y) override;
    void subsurface_place_above(Resource *resource, struct ::wl_resource *sibling_resource) override;
    void subsurface_place_below(Resource *resource, struct ::wl_resource *sibling_resource) override;
    void subsurface_set_sync(Resource *resource) override;
    void subsurface_set_desync(Resource *resource) override;
};

} // namespace KWin
