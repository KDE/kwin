/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "contrast_interface.h"
#include "region_interface.h"
#include "display.h"
#include "global_p.h"
#include "resource_p.h"
#include "surface_interface_p.h"

#include <wayland-server.h>
#include <wayland-contrast-server-protocol.h>

namespace KWayland
{
namespace Server
{

class ContrastManagerInterface::Private : public Global::Private
{
public:
    Private(ContrastManagerInterface *q, Display *d);

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void createContrast(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface);

    static void createCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface);
    static void unsetCallback(wl_client *client, wl_resource *resource, wl_resource *surface);
    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        auto contrastManager = reinterpret_cast<QPointer<ContrastManagerInterface>*>(wl_resource_get_user_data(r))->data();
        if (contrastManager) {
            return static_cast<Private*>(contrastManager->d.data());
         }
        return nullptr;
    }

    ContrastManagerInterface *q;
    static const struct org_kde_kwin_contrast_manager_interface s_interface;
    static const quint32 s_version;
};

const quint32 ContrastManagerInterface::Private::s_version = 1;

#ifndef K_DOXYGEN
const struct org_kde_kwin_contrast_manager_interface ContrastManagerInterface::Private::s_interface = {
    createCallback,
    unsetCallback
};
#endif

ContrastManagerInterface::Private::Private(ContrastManagerInterface *q, Display *d)
    : Global::Private(d, &org_kde_kwin_contrast_manager_interface, s_version)
    , q(q)
{
}

void ContrastManagerInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&org_kde_kwin_contrast_manager_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    auto ref = new QPointer<ContrastManagerInterface>(q);//deleted in unbind
    wl_resource_set_implementation(resource, &s_interface, ref, unbind);
}

void ContrastManagerInterface::Private::unbind(wl_resource *resource)
{
    delete reinterpret_cast<QPointer<ContrastManagerInterface>*>(wl_resource_get_user_data(resource));
}

void ContrastManagerInterface::Private::createCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface)
{
    auto m = cast(resource);
    if (!m) {
        return;// will happen if global is deleted
    }
    m->createContrast(client, resource, id, surface);
}

void ContrastManagerInterface::Private::createContrast(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        return;
    }

    ContrastInterface *contrast = new ContrastInterface(q, resource);
    contrast->create(display->getConnection(client), wl_resource_get_version(resource), id);
    if (!contrast->resource()) {
        wl_resource_post_no_memory(resource);
        delete contrast;
        return;
    }
    s->d_func()->setContrast(QPointer<ContrastInterface>(contrast));
}

void ContrastManagerInterface::Private::unsetCallback(wl_client *client, wl_resource *resource, wl_resource *surface)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        return;
    }
    s->d_func()->setContrast(QPointer<ContrastInterface>());
}

ContrastManagerInterface::ContrastManagerInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

ContrastManagerInterface::~ContrastManagerInterface() = default;

class ContrastInterface::Private : public Resource::Private
{
public:
    Private(ContrastInterface *q, ContrastManagerInterface *c, wl_resource *parentResource);
    ~Private();

    QRegion pendingRegion;
    QRegion currentRegion;
    qreal pendingContrast;
    qreal currentContrast;
    qreal pendingIntensity;
    qreal currentIntensity;
    qreal pendingSaturation;
    qreal currentSaturation;

private:
    void commit();
    //TODO
    ContrastInterface *q_func() {
        return reinterpret_cast<ContrastInterface *>(q);
    }

    static void commitCallback(wl_client *client, wl_resource *resource);
    static void setRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region);
    static void setContrastCallback(wl_client *client, wl_resource *resource, wl_fixed_t contrast);
    static void setIntensityCallback(wl_client *client, wl_resource *resource, wl_fixed_t intensity);
    static void setSaturationCallback(wl_client *client, wl_resource *resource, wl_fixed_t saturation);

    static const struct org_kde_kwin_contrast_interface s_interface;
};

#ifndef K_DOXYGEN
const struct org_kde_kwin_contrast_interface ContrastInterface::Private::s_interface = {
    commitCallback,
    setRegionCallback,
    setContrastCallback,
    setIntensityCallback,
    setSaturationCallback,
    resourceDestroyedCallback
};
#endif

void ContrastInterface::Private::commitCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    cast<Private>(resource)->commit();
}

void ContrastInterface::Private::commit()
{
    currentRegion = pendingRegion;
    currentContrast = pendingContrast;
    currentIntensity = pendingIntensity;
    currentSaturation = pendingSaturation;
}

void ContrastInterface::Private::setRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region)
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

void ContrastInterface::Private::setContrastCallback(wl_client *client, wl_resource *resource, wl_fixed_t contrast)
{
    Q_UNUSED(client)
    Private *p = cast<Private>(resource);
    p->pendingContrast = wl_fixed_to_double(contrast);
}

void ContrastInterface::Private::setIntensityCallback(wl_client *client, wl_resource *resource, wl_fixed_t intensity)
{
    Q_UNUSED(client)
    Private *p = cast<Private>(resource);
    p->pendingIntensity = wl_fixed_to_double(intensity);
}

void ContrastInterface::Private::setSaturationCallback(wl_client *client, wl_resource *resource, wl_fixed_t saturation)
{
    Q_UNUSED(client)
    Private *p = cast<Private>(resource);
    p->pendingSaturation = wl_fixed_to_double(saturation);
}

ContrastInterface::Private::Private(ContrastInterface *q, ContrastManagerInterface *c, wl_resource *parentResource)
    : Resource::Private(q, c, parentResource, &org_kde_kwin_contrast_interface, &s_interface)
{
}

ContrastInterface::Private::~Private() = default;

ContrastInterface::ContrastInterface(ContrastManagerInterface *parent, wl_resource *parentResource)
    : Resource(new Private(this, parent, parentResource))
{
}

ContrastInterface::~ContrastInterface() = default;

QRegion ContrastInterface::region() const
{
    Q_D();
    return d->currentRegion;
}

qreal ContrastInterface::contrast() const
{
    Q_D();
    return d->currentContrast;
}

qreal ContrastInterface::intensity() const
{
    Q_D();
    return d->currentIntensity;
}

qreal ContrastInterface::saturation() const
{
    Q_D();
    return d->currentSaturation;
}

ContrastInterface::Private *ContrastInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}
