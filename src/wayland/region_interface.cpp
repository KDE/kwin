/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "region_interface_p.h"
#include "utils.h"

namespace KWaylandServer
{
RegionInterface::RegionInterface(wl_resource *resource)
    : QtWaylandServer::wl_region(resource)
{
}

void RegionInterface::region_destroy_resource(Resource *)
{
    delete this;
}

void RegionInterface::region_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void RegionInterface::region_add(Resource *, int32_t x, int32_t y, int32_t width, int32_t height)
{
    m_region += QRegion(x, y, width, height);
}

void RegionInterface::region_subtract(Resource *, int32_t x, int32_t y, int32_t width, int32_t height)
{
    m_region -= QRegion(x, y, width, height);
}

QRegion RegionInterface::region() const
{
    return m_region;
}

RegionInterface *RegionInterface::get(wl_resource *native)
{
    return resource_cast<RegionInterface *>(native);
}

} // namespace KWaylandServer
