/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "viewporter_interface.h"
#include "display.h"
#include "surface_interface_p.h"
#include "viewporter_interface_p.h"

static const int s_version = 1;

namespace KWaylandServer
{
class ViewporterInterfacePrivate : public QtWaylandServer::wp_viewporter
{
protected:
    void wp_viewporter_destroy(Resource *resource) override;
    void wp_viewporter_get_viewport(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
};

void ViewporterInterfacePrivate::wp_viewporter_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ViewporterInterfacePrivate::wp_viewporter_get_viewport(Resource *resource, uint32_t id, struct ::wl_resource *surface_resource)
{
    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);
    ViewportInterface *viewport = ViewportInterface::get(surface);

    if (viewport) {
        wl_resource_post_error(resource->handle, error_viewport_exists, "the specified surface already has a viewport");
        return;
    }

    wl_resource *viewportResource = wl_resource_create(resource->client(), &wp_viewport_interface, resource->version(), id);

    new ViewportInterface(surface, viewportResource);
}

ViewportInterface::ViewportInterface(SurfaceInterface *surface, wl_resource *resource)
    : QtWaylandServer::wp_viewport(resource)
    , surface(surface)
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    surfacePrivate->viewportExtension = this;
}

ViewportInterface::~ViewportInterface()
{
    if (surface) {
        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
        surfacePrivate->viewportExtension = nullptr;
    }
}

ViewportInterface *ViewportInterface::get(SurfaceInterface *surface)
{
    return SurfaceInterfacePrivate::get(surface)->viewportExtension;
}

void ViewportInterface::wp_viewport_destroy_resource(Resource *resource)
{
    delete this;
}

void ViewportInterface::wp_viewport_destroy(Resource *resource)
{
    if (surface) {
        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
        surfacePrivate->pending.viewport.sourceGeometry = QRectF();
        surfacePrivate->pending.viewport.sourceGeometryIsSet = true;
        surfacePrivate->pending.viewport.destinationSize = QSize();
        surfacePrivate->pending.viewport.destinationSizeIsSet = true;
    }

    wl_resource_destroy(resource->handle);
}

void ViewportInterface::wp_viewport_set_source(Resource *resource, wl_fixed_t x_fixed, wl_fixed_t y_fixed, wl_fixed_t width_fixed, wl_fixed_t height_fixed)
{
    if (!surface) {
        wl_resource_post_error(resource->handle, error_no_surface, "the wl_surface for this viewport no longer exists");
        return;
    }

    const qreal x = wl_fixed_to_double(x_fixed);
    const qreal y = wl_fixed_to_double(y_fixed);
    const qreal width = wl_fixed_to_double(width_fixed);
    const qreal height = wl_fixed_to_double(height_fixed);

    if (x == -1 && y == -1 && width == -1 && height == -1) {
        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
        surfacePrivate->pending.viewport.sourceGeometry = QRectF();
        surfacePrivate->pending.viewport.sourceGeometryIsSet = true;
        return;
    }

    if (x < 0 || y < 0 || width <= 0 || height <= 0) {
        wl_resource_post_error(resource->handle, error_bad_value, "invalid source geometry");
        return;
    }

    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    surfacePrivate->pending.viewport.sourceGeometry = QRectF(x, y, width, height);
    surfacePrivate->pending.viewport.sourceGeometryIsSet = true;
}

void ViewportInterface::wp_viewport_set_destination(Resource *resource, int32_t width, int32_t height)
{
    if (!surface) {
        wl_resource_post_error(resource->handle, error_no_surface, "the wl_surface for this viewport no longer exists");
        return;
    }

    if (width == -1 && height == -1) {
        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
        surfacePrivate->pending.viewport.destinationSize = QSize();
        surfacePrivate->pending.viewport.destinationSizeIsSet = true;
        return;
    }

    if (width <= 0 || height <= 0) {
        wl_resource_post_error(resource->handle, error_bad_value, "invalid destination size");
        return;
    }

    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    surfacePrivate->pending.viewport.destinationSize = QSize(width, height);
    surfacePrivate->pending.viewport.destinationSizeIsSet = true;
}

ViewporterInterface::ViewporterInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new ViewporterInterfacePrivate)
{
    d->init(*display, s_version);
}

ViewporterInterface::~ViewporterInterface()
{
}

} // namespace KWaylandServer
