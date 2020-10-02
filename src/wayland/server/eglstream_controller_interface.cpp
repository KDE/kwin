/*
    SPDX-FileCopyrightText: 2019 NVIDIA Inc.

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "eglstream_controller_interface.h"
#include "clientconnection.h"
#include "display.h"
#include "logging.h"

#include <wayland-util.h>
#include <qwayland-server-wl-eglstream-controller.h>

namespace KWaylandServer
{

static const quint32 s_version = 1;

class EglStreamControllerInterfacePrivate : public QtWaylandServer::wl_eglstream_controller
{
public:
    EglStreamControllerInterfacePrivate(EglStreamControllerInterface *controller, Display *display);

    EglStreamControllerInterface *q;

protected:
    void eglstream_controller_attach_eglstream_consumer(Resource *resource, wl_resource *wl_surface, wl_resource *eglStream) override;
    void eglstream_controller_attach_eglstream_consumer_attribs(Resource *resource, wl_resource *wl_surface, wl_resource *eglStream, wl_array *attribs) override;

};

void EglStreamControllerInterfacePrivate::eglstream_controller_attach_eglstream_consumer(Resource *resource, wl_resource *wl_surface, wl_resource *eglStream)
{
    wl_array noAttribs = { 0, 0, nullptr };
    eglstream_controller_attach_eglstream_consumer_attribs(resource, wl_surface, eglStream, &noAttribs);
}

void EglStreamControllerInterfacePrivate::eglstream_controller_attach_eglstream_consumer_attribs(Resource *resource, wl_resource *wl_surface, wl_resource *eglStream, wl_array *attribs)
{
    SurfaceInterface *surface = SurfaceInterface::get(wl_surface);
    if (!surface) {
        wl_resource_post_error(resource->handle, 0, "Invalid  surface");
        return;
    }
    emit q->streamConsumerAttached(surface, eglStream, attribs);
}

EglStreamControllerInterfacePrivate::EglStreamControllerInterfacePrivate(EglStreamControllerInterface *_q, Display *display)
    : QtWaylandServer::wl_eglstream_controller(*display, s_version) 
    , q(_q)
{
}

EglStreamControllerInterface::~EglStreamControllerInterface() = default;

EglStreamControllerInterface::EglStreamControllerInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new EglStreamControllerInterfacePrivate(this, display))
{
}

}
