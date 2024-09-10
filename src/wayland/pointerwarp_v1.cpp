/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "pointerwarp_v1.h"
#include "display.h"
#include "pointer.h"
#include "surface.h"

namespace KWin
{

static constexpr uint32_t s_version = 1;

PointerWarpV1::PointerWarpV1(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::wp_pointer_warp_v1(*display, s_version)
{
}

void PointerWarpV1::wp_pointer_warp_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PointerWarpV1::wp_pointer_warp_v1_warp_pointer(Resource *resource, ::wl_resource *surface, ::wl_resource *pointer, wl_fixed_t x, wl_fixed_t y, uint32_t serial)
{
    Q_EMIT warpRequested(SurfaceInterface::get(surface), PointerInterface::get(pointer), QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)));
}

}
