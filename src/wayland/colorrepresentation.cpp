#include "colorrepresentation.h"
#include "display.h"
#include "surface.h"
#include "surface_p.h"

namespace KWin
{

ColorRepresentationManager::ColorRepresentationManager(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::wp_color_representation_manager_v1(*display, 1)
{
}

void ColorRepresentationManager::wp_color_representation_manager_v1_create(Resource *resource, uint32_t color_representation, wl_resource *surface)
{
    new ColorRepresentationSurface(SurfaceInterface::get(surface), resource->client(), color_representation);
}

void ColorRepresentationManager::wp_color_representation_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

ColorRepresentationSurface::ColorRepresentationSurface(SurfaceInterface *surf, wl_client *client, int id)
    : QtWaylandServer::wp_color_representation_v1(client, id, 1)
    , m_surface(surf)
{
}

void ColorRepresentationSurface::wp_color_representation_v1_set_alpha_mode(Resource *resource, uint32_t alpha_mode)
{
    // ignored for now
}

void ColorRepresentationSurface::wp_color_representation_v1_set_coefficients(Resource *resource, uint32_t code_point)
{
    // not supported
}

void ColorRepresentationSurface::wp_color_representation_v1_set_chroma_location(Resource *resource, uint32_t code_point)
{
    // not supported
}

void ColorRepresentationSurface::wp_color_representation_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ColorRepresentationSurface::wp_color_representation_v1_destroy_resource(Resource *resource)
{
    delete this;
}
}
