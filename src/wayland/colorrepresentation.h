#pragma once

#include "qwayland-server-color-representation-v1.h"
#include "surface.h"

#include <QObject>

namespace KWin
{

class Display;
class SurfaceInterface;

class ColorRepresentationManager : public QObject, public QtWaylandServer::wp_color_representation_manager_v1
{
    Q_OBJECT
public:
    ColorRepresentationManager(Display *display, QObject *parent);

private:
    void wp_color_representation_manager_v1_create(Resource *resource, uint32_t color_representation, wl_resource *surface) override;
    void wp_color_representation_manager_v1_destroy(Resource *resource) override;
};

class ColorRepresentationSurface : public QObject, public QtWaylandServer::wp_color_representation_v1
{
    Q_OBJECT
public:
    ColorRepresentationSurface(SurfaceInterface *surf, wl_client *client, int id);

private:
    void wp_color_representation_v1_set_alpha_mode(Resource *resource, uint32_t alpha_mode) override;
    void wp_color_representation_v1_set_coefficients(Resource *resource, uint32_t code_point) override;
    void wp_color_representation_v1_set_chroma_location(Resource *resource, uint32_t code_point) override;
    void wp_color_representation_v1_destroy(Resource *resource) override;
    void wp_color_representation_v1_destroy_resource(Resource *resource) override;

    SurfaceInterface *const m_surface;
};

}
