/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <QRegion>

#include "qwayland-server-wayland.h"

namespace KWaylandServer
{
class RegionInterface : public QtWaylandServer::wl_region
{
public:
    static RegionInterface *get(wl_resource *native);
    explicit RegionInterface(wl_resource *resource);

    QRegion region() const;

protected:
    void region_destroy_resource(Resource *resource) override;
    void region_destroy(Resource *resource) override;
    void region_add(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void region_subtract(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;

private:
    QRegion m_region;
};

} // namespace KWaylandServer
