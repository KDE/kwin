/*
    SPDX-FileCopyrightText: 2019 NVIDIA Inc.

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "eglstream_controller_interface_p.h"
#include "clientconnection.h"
#include "display.h"
#include "logging.h"

#include <wayland-util.h>
#include <QLibrary>

namespace KWayland
{
namespace Server
{

const quint32 EglStreamControllerInterface::Private::s_version = 1;

#ifndef K_DOXYGEN
const struct wl_eglstream_controller_interface EglStreamControllerInterface::Private::s_interface = {
    attachStreamConsumer,
    attachStreamConsumerAttribs
};
#endif

void EglStreamControllerInterface::Private::attachStreamConsumer(wl_client *client,
                                                                 wl_resource *resource,
                                                                 wl_resource *surface,
                                                                 wl_resource *eglStream)
{
    wl_array noAttribs = { 0, 0, nullptr };
    attachStreamConsumerAttribs(client, resource, surface, eglStream, &noAttribs);
}

void EglStreamControllerInterface::Private::attachStreamConsumerAttribs(wl_client *client,
                                                                        wl_resource *resource,
                                                                        wl_resource *surface,
                                                                        wl_resource *eglStream,
                                                                        wl_array *attribs)
{
    Q_UNUSED(client);
    Private *p = reinterpret_cast<Private *>(wl_resource_get_user_data(resource));
    emit p->q->streamConsumerAttached(SurfaceInterface::get(surface), eglStream, attribs);
}

EglStreamControllerInterface::Private::Private(EglStreamControllerInterface *q, Display *display)
    // libnvidia-egl-wayland.so.1 may not be present on all systems, so we load it dynamically
    : Global::Private(display,
                      reinterpret_cast<wl_interface *>(QLibrary::resolve(QLatin1String("libnvidia-egl-wayland.so.1"),
                                                        "wl_eglstream_controller_interface")),
                      s_version)
    , q(q)
{
}

void EglStreamControllerInterface::Private::create()
{
    // bail out early if we were unable to load the interface
    if (m_interface == nullptr) {
        qCWarning(KWAYLAND_SERVER) << "failed to resolve wl_eglstream_controller_interface";
        return;
    }

    Global::Private::create();
}

void EglStreamControllerInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    wl_resource *r = display->getConnection(client)->createResource(m_interface, version, id);
    if (r == nullptr) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(r, &s_interface, this, nullptr);
}

EglStreamControllerInterface::~EglStreamControllerInterface() = default;

EglStreamControllerInterface::EglStreamControllerInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

void EglStreamControllerInterface::create()
{
    static_cast<Private &>(*d).create();
}

}
}
