/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "qwayland-server-viewporter.h"

#include <QPointer>

namespace KWaylandServer
{

class SurfaceInterface;

class ViewportInterface : public QtWaylandServer::wp_viewport
{
public:
    ViewportInterface(SurfaceInterface *surface, wl_resource *resource);
    ~ViewportInterface() override;

    static ViewportInterface *get(SurfaceInterface *surface);

    QPointer<SurfaceInterface> surface;

protected:
    void wp_viewport_destroy_resource(Resource *resource) override;
    void wp_viewport_destroy(Resource *resource) override;
    void wp_viewport_set_source(Resource *resource, wl_fixed_t x, wl_fixed_t y, wl_fixed_t width, wl_fixed_t height) override;
    void wp_viewport_set_destination(Resource *resource, int32_t width, int32_t height) override;
};

} // namespace KWaylandServer
