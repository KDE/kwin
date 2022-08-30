/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <QRegion>

#include <wayland-server-protocol.h>

namespace KWaylandServer
{
class RegionInterface
{
public:
    static RegionInterface *fromResource(wl_resource *resource);
    explicit RegionInterface(wl_client *client, int version, uint32_t id);

    QRegion m_region;

    static void region_destroy_resource(wl_resource *resource);
    static void region_destroy(wl_client *client, wl_resource *resource);
    static void region_add(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height);
    static void region_subtract(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height);

    static constexpr struct wl_region_interface implementation = {
        .destroy = region_destroy,
        .add = region_add,
        .subtract = region_subtract,
    };
};

} // namespace KWaylandServer
