/****************************************************************************
Copyright 2019 NVIDIA Inc.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
****************************************************************************/
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

#ifndef DOXYGEN_SHOULD_SKIP_THIS
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
                      (wl_interface *)QLibrary::resolve(QLatin1String("libnvidia-egl-wayland.so.1"),
                                                        "wl_eglstream_controller_interface"),
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
