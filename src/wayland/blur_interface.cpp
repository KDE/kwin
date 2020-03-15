/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "blur_interface.h"
#include "region_interface.h"
#include "display.h"
#include "global_p.h"
#include "resource_p.h"
#include "surface_interface_p.h"

#include <wayland-server.h>
#include <wayland-blur-server-protocol.h>

namespace KWayland
{
namespace Server
{

class BlurManagerInterface::Private : public Global::Private
{
public:
    Private(BlurManagerInterface *q, Display *d);

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void createBlur(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface);

    static void createCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface);
    static void unsetCallback(wl_client *client, wl_resource *resource, wl_resource *surface);
    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        auto blurManager = reinterpret_cast<QPointer<BlurManagerInterface>*>(wl_resource_get_user_data(r))->data();
        if (blurManager) {
            return static_cast<Private*>(blurManager->d.data());
        }
        return nullptr;
    }
    BlurManagerInterface *q;
    static const struct org_kde_kwin_blur_manager_interface s_interface;
    static const quint32 s_version;
};

const quint32 BlurManagerInterface::Private::s_version = 1;

#ifndef K_DOXYGEN
const struct org_kde_kwin_blur_manager_interface BlurManagerInterface::Private::s_interface = {
    createCallback,
    unsetCallback
};
#endif

BlurManagerInterface::Private::Private(BlurManagerInterface *q, Display *d)
    : Global::Private(d, &org_kde_kwin_blur_manager_interface, s_version)
    , q(q)
{
}

void BlurManagerInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&org_kde_kwin_blur_manager_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    auto ref = new QPointer<BlurManagerInterface>(q);//deleted in unbind
    wl_resource_set_implementation(resource, &s_interface, ref, unbind);
}

void BlurManagerInterface::Private::unbind(wl_resource *r)
{
    delete reinterpret_cast<QPointer<BlurManagerInterface>*>(wl_resource_get_user_data(r));
}

void BlurManagerInterface::Private::createCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface)
{
    auto m = cast(resource);
    if (!m) {
        return;// will happen if global is deleted
    }
    m->createBlur(client, resource, id, surface);
}

void BlurManagerInterface::Private::createBlur(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        return;
    }

    BlurInterface *blur = new BlurInterface(q, resource);
    blur->create(display->getConnection(client), wl_resource_get_version(resource), id);
    if (!blur->resource()) {
        wl_resource_post_no_memory(resource);
        delete blur;
        return;
    }
    s->d_func()->setBlur(QPointer<BlurInterface>(blur));
}

void BlurManagerInterface::Private::unsetCallback(wl_client *client, wl_resource *resource, wl_resource *surface)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        return;
    }
    s->d_func()->setBlur(QPointer<BlurInterface>());
}

BlurManagerInterface::BlurManagerInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

BlurManagerInterface::~BlurManagerInterface() = default;

class BlurInterface::Private : public Resource::Private
{
public:
    Private(BlurInterface *q, BlurManagerInterface *c, wl_resource *parentResource);
    ~Private();

    QRegion pendingRegion;
    QRegion currentRegion;

private:
    void commit();
    //TODO
    BlurInterface *q_func() {
        return reinterpret_cast<BlurInterface *>(q);
    }

    static void commitCallback(wl_client *client, wl_resource *resource);
    static void setRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region);

    static const struct org_kde_kwin_blur_interface s_interface;
};

#ifndef K_DOXYGEN
const struct org_kde_kwin_blur_interface BlurInterface::Private::s_interface = {
    commitCallback,
    setRegionCallback,
    resourceDestroyedCallback
};
#endif

void BlurInterface::Private::commitCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    cast<Private>(resource)->commit();
}

void BlurInterface::Private::commit()
{
    currentRegion = pendingRegion;
}

void BlurInterface::Private::setRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region)
{
    Q_UNUSED(client)
    Private *p = cast<Private>(resource);
    RegionInterface *r = RegionInterface::get(region);
    if (r) {
        p->pendingRegion = r->region();
    } else {
        p->pendingRegion = QRegion();
    }
}

BlurInterface::Private::Private(BlurInterface *q, BlurManagerInterface *c, wl_resource *parentResource)
    : Resource::Private(q, c, parentResource, &org_kde_kwin_blur_interface, &s_interface)
{
}

BlurInterface::Private::~Private() = default;

BlurInterface::BlurInterface(BlurManagerInterface *parent, wl_resource *parentResource)
    : Resource(new Private(this, parent, parentResource))
{
}

BlurInterface::~BlurInterface() = default;

QRegion BlurInterface::region()
{
    Q_D();
    return d->currentRegion;
}

BlurInterface::Private *BlurInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}
