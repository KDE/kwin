/*
    SPDX-FileCopyrightText: 2015 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "slide_interface.h"
#include "display.h"
#include "global_p.h"
#include "surface_interface.h"
#include "resource_p.h"
#include "surface_interface_p.h"

#include <wayland-server.h>
#include <wayland-slide-server-protocol.h>

namespace KWayland
{
namespace Server
{

class SlideManagerInterface::Private : public Global::Private
{
public:
    Private(SlideManagerInterface *q, Display *d);

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void createSlide(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface);

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    static void createCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * surface);
    static void unsetCallback(wl_client *client, wl_resource *resource, wl_resource * surface);

    SlideManagerInterface *q;
    static const struct org_kde_kwin_slide_manager_interface s_interface;
    //initializing here doesn't link
    static const quint32 s_version;
};

const quint32 SlideManagerInterface::Private::s_version = 1;

#ifndef K_DOXYGEN
const struct org_kde_kwin_slide_manager_interface SlideManagerInterface::Private::s_interface = {
    createCallback,
    unsetCallback
};
#endif

void SlideManagerInterface::Private::createCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * surface)
{
    cast(resource)->createSlide(client, resource, id, surface);
}

void SlideManagerInterface::Private::createSlide(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        return;
    }

    SlideInterface *slide = new SlideInterface(q, resource);
    slide->create(display->getConnection(client), wl_resource_get_version(resource), id);
    if (!slide->resource()) {
        wl_resource_post_no_memory(resource);
        delete slide;
        return;
    }
    s->d_func()->setSlide(QPointer<SlideInterface>(slide));
}

void SlideManagerInterface::Private::unsetCallback(wl_client *client, wl_resource *resource, wl_resource * surface)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    Q_UNUSED(surface)
    // TODO: implement
}

SlideManagerInterface::Private::Private(SlideManagerInterface *q, Display *d)
    : Global::Private(d, &org_kde_kwin_slide_manager_interface, s_version)
    , q(q)
{
}

void SlideManagerInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&org_kde_kwin_slide_manager_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    // TODO: should we track?
}

void SlideManagerInterface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
    // TODO: implement?
}

SlideManagerInterface::SlideManagerInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

SlideManagerInterface::~SlideManagerInterface() = default;

class SlideInterface::Private : public Resource::Private
{
public:
    Private(SlideInterface *q, SlideManagerInterface *c, wl_resource *parentResource);
    ~Private();

    SlideInterface::Location pendingLocation;
    SlideInterface::Location currentLocation;
    uint32_t pendingOffset;
    uint32_t currentOffset;

private:
    static void commitCallback(wl_client *client, wl_resource *resource);
    static void setLocationCallback(wl_client *client, wl_resource *resource, uint32_t location);
    static void setOffsetCallback(wl_client *client, wl_resource *resource, int32_t offset);

    SlideInterface *q_func() {
        return reinterpret_cast<SlideInterface *>(q);
    }

    static const struct org_kde_kwin_slide_interface s_interface;
};

#ifndef K_DOXYGEN
const struct org_kde_kwin_slide_interface SlideInterface::Private::s_interface = {
    commitCallback,
    setLocationCallback,
    setOffsetCallback,
    resourceDestroyedCallback
};
#endif

void SlideInterface::Private::commitCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    Private *p = cast<Private>(resource);
    p->currentLocation = p->pendingLocation;
    p->currentOffset = p->pendingOffset;
}

void SlideInterface::Private::setLocationCallback(wl_client *client, wl_resource *resource, uint32_t location)
{
    Q_UNUSED(client)
    Private *p = cast<Private>(resource);
    p->pendingLocation = (SlideInterface::Location)location;
}

void SlideInterface::Private::setOffsetCallback(wl_client *client, wl_resource *resource, int32_t offset)
{
    Q_UNUSED(client)
    Private *p = cast<Private>(resource);
    p->pendingOffset = offset;
}

SlideInterface::Private::Private(SlideInterface *q, SlideManagerInterface *c, wl_resource *parentResource)
    : Resource::Private(q, c, parentResource, &org_kde_kwin_slide_interface, &s_interface)
{
}

SlideInterface::Private::~Private() = default;

SlideInterface::SlideInterface(SlideManagerInterface *parent, wl_resource *parentResource)
    : Resource(new Private(this, parent, parentResource))
{
}

SlideInterface::~SlideInterface() = default;


SlideInterface::Location SlideInterface::location() const
{
    Q_D();
    return d->currentLocation;
}

qint32 SlideInterface::offset() const
{
    Q_D();
    return d->currentOffset;
}

SlideInterface::Private *SlideInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}

