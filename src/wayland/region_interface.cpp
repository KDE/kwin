/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "region_interface_p.h"

namespace KWaylandServer
{
RegionInterface::RegionInterface(wl_client *client, int version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(client, &wl_region_interface, version, id);
    wl_resource_set_implementation(resource, &implementation, this, region_destroy_resource);
}

void RegionInterface::region_destroy_resource(wl_resource *resource)
{
    delete RegionInterface::fromResource(resource);
}

void RegionInterface::region_destroy(wl_client *client, wl_resource *resource)
{
    wl_resource_destroy(resource);
}

void RegionInterface::region_add(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    auto region = RegionInterface::fromResource(resource);
    region->m_region += QRegion(x, y, width, height);
}

void RegionInterface::region_subtract(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    auto region = RegionInterface::fromResource(resource);
    region->m_region -= QRegion(x, y, width, height);
}

RegionInterface *RegionInterface::fromResource(wl_resource *resource)
{
    Q_ASSERT(wl_resource_instance_of(resource, &wl_region_interface, &implementation));
    return static_cast<RegionInterface *>(wl_resource_get_user_data(resource));
}

} // namespace KWaylandServer
