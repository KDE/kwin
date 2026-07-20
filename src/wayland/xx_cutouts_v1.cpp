/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "xx_cutouts_v1.h"

#include "display.h"
#include "surface_p.h"

namespace KWin
{

CutoutsManagerV1::CutoutsManagerV1(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::xx_cutouts_manager_v1(*display, 1)
{
}

void CutoutsManagerV1::xx_cutouts_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void CutoutsManagerV1::xx_cutouts_manager_v1_get_cutouts(Resource *resource, uint32_t id, struct ::wl_resource *surface)
{
    const auto surf = SurfaceInterface::get(surface);
    const auto priv = SurfaceInterfacePrivate::get(surf);
    if (priv->cutouts) {
        // TODO add a protocol error for this.
        return;
    }
    priv->cutouts = new CutoutsV1(resource->client(), id, resource->version(), surf);
    Q_EMIT surf->cutoutsCreated();
}

CutoutsV1::CutoutsV1(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface)
    : QtWaylandServer::xx_cutouts_v1(client, id, version)
    , m_surface(surface)
{
}

CutoutsV1::~CutoutsV1()
{
    if (m_surface) {
        SurfaceInterfacePrivate::get(m_surface)->cutouts = nullptr;
    }
}

void CutoutsV1::setCutouts(const QList<RectF> &cutouts, BorderRadius corners)
{
    uint32_t id = 1;
    for (const RectF &box : cutouts) {
        const Rect surfaceLocal = box.scaled(1 / m_surface->compositorToClientScale()).rounded();
        if (surfaceLocal.isEmpty()) {
            continue;
        }
        send_cutout_box(surfaceLocal.x(), surfaceLocal.y(), surfaceLocal.width(), surfaceLocal.height(), type_cutout, id++);
    }
    if (corners.topLeft()) {
        send_cutout_corner(corner_position_top_left, std::round(corners.topLeft() / m_surface->compositorToClientScale()), id++);
    }
    if (corners.topRight()) {
        send_cutout_corner(corner_position_top_right, std::round(corners.topRight() / m_surface->compositorToClientScale()), id++);
    }
    if (corners.bottomRight()) {
        send_cutout_corner(corner_position_bottom_right, std::round(corners.bottomRight() / m_surface->compositorToClientScale()), id++);
    }
    if (corners.bottomLeft()) {
        send_cutout_corner(corner_position_bottom_left, std::round(corners.bottomLeft() / m_surface->compositorToClientScale()), id++);
    }
    send_configure();
}

void CutoutsV1::xx_cutouts_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void CutoutsV1::xx_cutouts_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void CutoutsV1::xx_cutouts_v1_set_unhandled(Resource *resource, wl_array *unhandled)
{
    // TODO currently unused
}

}
