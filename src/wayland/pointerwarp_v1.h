/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "qwayland-server-pointer-warp-v1.h"

#include <QObject>
#include <QPointF>

namespace KWin
{

class Display;
class SurfaceInterface;
class PointerInterface;

class PointerWarpV1 : public QObject, public QtWaylandServer::wp_pointer_warp_v1
{
    Q_OBJECT
public:
    explicit PointerWarpV1(Display *display, QObject *parent);

Q_SIGNALS:
    void warpRequested(SurfaceInterface *surface, PointerInterface *pointer, QPointF point);

private:
    void wp_pointer_warp_v1_destroy(Resource *resource) override;
    void wp_pointer_warp_v1_warp_pointer(Resource *resource, ::wl_resource *surface, ::wl_resource *pointer, wl_fixed_t x, wl_fixed_t y, uint32_t serial) override;
};

}
