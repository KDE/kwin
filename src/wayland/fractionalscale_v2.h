/*
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <QPointer>
#include <QObject>
#include <qwayland-server-fractional-scale-v2.h>

namespace KWin
{

class Display;
class FractionalScaleManagerV2Private;
class SurfaceInterface;

class FractionalScaleManagerV2 : public QObject, private QtWaylandServer::wp_fractional_scale_manager_v2
{
    Q_OBJECT
public:
    FractionalScaleManagerV2(Display *display, QObject *parent);
    ~FractionalScaleManagerV2();

private:
    void wp_fractional_scale_manager_v2_destroy_global() override;
    void wp_fractional_scale_manager_v2_get_fractional_scale(Resource *resource, uint32_t id, ::wl_resource *surface) override;

    FractionalScaleManagerV2Private *d;
};

class FractionalScaleV2 : private QtWaylandServer::wp_fractional_scale_v2
{
public:
    FractionalScaleV2(SurfaceInterface *surface, wl_resource *resource);

    void setFractionalScale(double scale);

private:
    void wp_fractional_scale_v2_set_scale_factor(Resource *resource, uint32_t scale_8_24) override;
    void wp_fractional_scale_v2_destroy(Resource *resource) override;

    const QPointer<SurfaceInterface> m_surface;
};

}
