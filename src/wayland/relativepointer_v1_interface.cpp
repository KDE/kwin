/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "relativepointer_v1_interface.h"
#include "display.h"
#include "pointer_interface_p.h"
#include "relativepointer_v1_interface_p.h"

namespace KWaylandServer
{

static const int s_version = 1;

RelativePointerManagerV1InterfacePrivate::RelativePointerManagerV1InterfacePrivate(Display *display)
    : QtWaylandServer::zwp_relative_pointer_manager_v1(*display, s_version)
{
}

void RelativePointerManagerV1InterfacePrivate::zwp_relative_pointer_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void RelativePointerManagerV1InterfacePrivate::zwp_relative_pointer_manager_v1_get_relative_pointer(Resource *resource, uint32_t id, struct ::wl_resource *pointer_resource)
{
    PointerInterface *pointer = PointerInterface::get(pointer_resource);
    if (!pointer) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "invalid pointer");
        return;
    }

    wl_resource *relativePointerResource = wl_resource_create(resource->client(),
                                                              &zwp_relative_pointer_v1_interface,
                                                              resource->version(), id);
    if (!relativePointerResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    new RelativePointerV1Interface(pointer, relativePointerResource);
}

RelativePointerManagerV1Interface::RelativePointerManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new RelativePointerManagerV1InterfacePrivate(display))
{
}

RelativePointerManagerV1Interface::~RelativePointerManagerV1Interface()
{
}

RelativePointerV1Interface::RelativePointerV1Interface(PointerInterface *pointer, ::wl_resource *resource)
    : QtWaylandServer::zwp_relative_pointer_v1(resource)
    , pointer(pointer)
{
    pointer->d_func()->registerRelativePointerV1(this);
}

RelativePointerV1Interface::~RelativePointerV1Interface()
{
    if (pointer) {
        pointer->d_func()->unregisterRelativePointerV1(this);
    }
}

void RelativePointerV1Interface::zwp_relative_pointer_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete this;
}

void RelativePointerV1Interface::zwp_relative_pointer_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

} // namespace KWaylandServer
