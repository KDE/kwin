/*
    SPDX-FileCopyrightText: 2015 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "slide_interface.h"
#include "display.h"
#include "surface_interface_p.h"

#include <wayland-server.h>
#include <qwayland-server-slide.h>

namespace KWaylandServer
{

static const quint32 s_version = 1;

class SlideManagerInterfacePrivate : public QtWaylandServer::org_kde_kwin_slide_manager
{
public:
    SlideManagerInterfacePrivate(SlideManagerInterface *_q, Display *display);

private:
    SlideManagerInterface *q;

protected:
    void org_kde_kwin_slide_manager_create(Resource *resource, uint32_t id, wl_resource *surface) override;
    void org_kde_kwin_slide_manager_unset(Resource *resource, wl_resource *surface) override;
};

void SlideManagerInterfacePrivate::org_kde_kwin_slide_manager_create(Resource *resource, uint32_t id, wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "Invalid  surface");
        return;
    }

    wl_resource *slide_resource = wl_resource_create(resource->client(), &org_kde_kwin_slide_interface, resource->version(), id);
    if (!slide_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto slide = new SlideInterface(q, slide_resource);
    
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(s);
    surfacePrivate->setSlide(QPointer<SlideInterface>(slide));
}

void SlideManagerInterfacePrivate::org_kde_kwin_slide_manager_unset(Resource *resource, wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "Invalid  surface");
        return;
    }
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(s);
    surfacePrivate->setSlide(QPointer<SlideInterface>());
}

SlideManagerInterfacePrivate::SlideManagerInterfacePrivate(SlideManagerInterface *_q, Display *display)
    : QtWaylandServer::org_kde_kwin_slide_manager(*display, s_version)
    , q(_q)
{
}

SlideManagerInterface::SlideManagerInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new SlideManagerInterfacePrivate(this, display))
{
}

SlideManagerInterface::~SlideManagerInterface() = default;

class SlideInterfacePrivate : public QtWaylandServer::org_kde_kwin_slide
{
public:
    SlideInterfacePrivate(SlideInterface *_q, wl_resource *resource);

    SlideInterface::Location pendingLocation;
    SlideInterface::Location currentLocation;
    uint32_t pendingOffset;
    uint32_t currentOffset;
    SlideInterface *q;

protected:
    void org_kde_kwin_slide_commit(Resource *resource) override;
    void org_kde_kwin_slide_set_location(Resource *resource, uint32_t location) override;
    void org_kde_kwin_slide_set_offset(Resource *resource, int32_t offset) override;
    void org_kde_kwin_slide_release(Resource *resource) override;
    void org_kde_kwin_slide_destroy_resource(Resource *resource) override;
};

void SlideInterfacePrivate::org_kde_kwin_slide_commit(Resource *resource)
{
    Q_UNUSED(resource)
    currentLocation = pendingLocation;
    currentOffset = pendingOffset;
}

void SlideInterfacePrivate::org_kde_kwin_slide_set_location(Resource *resource, uint32_t location)
{
    Q_UNUSED(resource)
    pendingLocation = (SlideInterface::Location)location;
}

void SlideInterfacePrivate::org_kde_kwin_slide_set_offset(Resource *resource, int32_t offset)
{
    Q_UNUSED(resource)
    pendingOffset = offset;
}

void SlideInterfacePrivate::org_kde_kwin_slide_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void SlideInterfacePrivate::org_kde_kwin_slide_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

SlideInterfacePrivate::SlideInterfacePrivate(SlideInterface *_q, wl_resource *resource)
    : QtWaylandServer::org_kde_kwin_slide(resource)
    , q(_q)
{
}

SlideInterface::SlideInterface(SlideManagerInterface *manager, wl_resource *resource)
    : QObject(manager)
    , d(new SlideInterfacePrivate(this, resource))
{
}

SlideInterface::~SlideInterface() = default;

SlideInterface::Location SlideInterface::location() const
{
    return d->currentLocation;
}

qint32 SlideInterface::offset() const
{
    return d->currentOffset;
}

}

