/*
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "tearingcontrol_v1_interface.h"
#include "display.h"
#include "surface_interface_p.h"

namespace KWaylandServer
{

static constexpr uint32_t s_version = 1;

class TearingControlManagerV1InterfacePrivate : public QtWaylandServer::wp_tearing_control_manager_v1
{
public:
    TearingControlManagerV1InterfacePrivate(Display *display);

private:
    void wp_tearing_control_manager_v1_destroy(Resource *resource) override;
    void wp_tearing_control_manager_v1_get_tearing_control(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
};

class TearingControlV1Interface : private QtWaylandServer::wp_tearing_control_v1
{
public:
    TearingControlV1Interface(SurfaceInterface *surface, wl_client *client, uint32_t id);
    ~TearingControlV1Interface();

private:
    void wp_tearing_control_v1_set_presentation_hint(Resource *resource, uint32_t hint) override;
    void wp_tearing_control_v1_destroy(Resource *resource) override;
    void wp_tearing_control_v1_destroy_resource(Resource *resource) override;

    const QPointer<SurfaceInterface> m_surface;
};

TearingControlManagerV1Interface::TearingControlManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new TearingControlManagerV1InterfacePrivate(display))
{
}

TearingControlManagerV1Interface::~TearingControlManagerV1Interface() = default;

TearingControlManagerV1InterfacePrivate::TearingControlManagerV1InterfacePrivate(Display *display)
    : QtWaylandServer::wp_tearing_control_manager_v1(*display, s_version)
{
}

void TearingControlManagerV1InterfacePrivate::wp_tearing_control_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void TearingControlManagerV1InterfacePrivate::wp_tearing_control_manager_v1_get_tearing_control(Resource *resource, uint32_t id, struct ::wl_resource *wlSurface)
{
    SurfaceInterface *surface = SurfaceInterface::get(wlSurface);
    if (SurfaceInterfacePrivate::get(surface)->tearing) {
        wl_resource_post_error(resource->handle, QtWaylandServer::wp_tearing_control_manager_v1::error_tearing_control_exists, "Surface already has a wp_surface_tearing_control_v1");
        return;
    }
    SurfaceInterfacePrivate::get(surface)->tearing = new TearingControlV1Interface(surface, resource->client(), id);
}

TearingControlV1Interface::TearingControlV1Interface(SurfaceInterface *surface, wl_client *client, uint32_t id)
    : QtWaylandServer::wp_tearing_control_v1(client, id, s_version)
    , m_surface(surface)
{
}

TearingControlV1Interface::~TearingControlV1Interface()
{
    if (m_surface) {
        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(m_surface);
        surfacePrivate->pending.presentationHint = PresentationHint::VSync;
        surfacePrivate->pending.tearingIsSet = true;
        surfacePrivate->tearing = nullptr;
    }
}

void TearingControlV1Interface::wp_tearing_control_v1_set_presentation_hint(Resource *resource, uint32_t hint)
{
    if (m_surface) {
        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(m_surface);
        if (hint == presentation_hint::presentation_hint_async) {
            surfacePrivate->pending.presentationHint = PresentationHint::Async;
        } else {
            surfacePrivate->pending.presentationHint = PresentationHint::VSync;
        }
        surfacePrivate->pending.tearingIsSet = true;
    }
}

void TearingControlV1Interface::wp_tearing_control_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void TearingControlV1Interface::wp_tearing_control_v1_destroy_resource(Resource *resource)
{
    delete this;
}
}
