/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <QObject>
#include <QPointer>
#include <qwayland-server-color-representation-v1.h>

namespace KWin
{

class Display;
class SurfaceInterface;

class ColorRepresentationManagerV1 : public QObject, private QtWaylandServer::wp_color_representation_manager_v1
{
    Q_OBJECT
public:
    explicit ColorRepresentationManagerV1(Display *display, QObject *parent);

private:
    void wp_color_representation_manager_v1_bind_resource(Resource *resource) override;
    void wp_color_representation_manager_v1_destroy(Resource *resource) override;
    void wp_color_representation_manager_v1_get_surface(Resource *resource, uint32_t color_representation, ::wl_resource *surface) override;
};

class ColorRepresentationSurfaceV1 : private QtWaylandServer::wp_color_representation_surface_v1
{
public:
    explicit ColorRepresentationSurfaceV1(SurfaceInterface *surface, wl_client *client, uint32_t id, uint32_t version);
    ~ColorRepresentationSurfaceV1();

private:
    void wp_color_representation_surface_v1_destroy_resource(Resource *resource) override;
    void wp_color_representation_surface_v1_destroy(Resource *resource) override;
    void wp_color_representation_surface_v1_set_alpha_mode(Resource *resource, uint32_t alpha_mode) override;
    void wp_color_representation_surface_v1_set_coefficients_and_range(Resource *resource, uint32_t coefficients, uint32_t range) override;
    void wp_color_representation_surface_v1_set_chroma_location(Resource *resource, uint32_t chroma_location) override;

    QPointer<SurfaceInterface> m_surface;
};

}
