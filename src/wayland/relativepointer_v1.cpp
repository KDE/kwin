/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "relativepointer_v1.h"
#include "clientconnection.h"
#include "display.h"
#include "pointer_p.h"
#include "relativepointer_v1_p.h"
#include "seat.h"
#include "surface.h"

namespace KWin
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

void RelativePointerManagerV1InterfacePrivate::zwp_relative_pointer_manager_v1_get_relative_pointer(Resource *resource,
                                                                                                    uint32_t id,
                                                                                                    struct ::wl_resource *pointer_resource)
{
    PointerInterface *pointer = PointerInterface::get(pointer_resource);
    if (!pointer) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT, "invalid pointer");
        return;
    }

    RelativePointerV1Interface *relativePointer = RelativePointerV1Interface::get(pointer);
    relativePointer->add(resource->client(), id, resource->version());
}

RelativePointerManagerV1Interface::RelativePointerManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new RelativePointerManagerV1InterfacePrivate(display))
{
}

RelativePointerManagerV1Interface::~RelativePointerManagerV1Interface()
{
}

RelativePointerV1Interface::RelativePointerV1Interface(PointerInterface *pointer)
    : pointer(pointer)
{
}

RelativePointerV1Interface *RelativePointerV1Interface::get(PointerInterface *pointer)
{
    if (pointer) {
        PointerInterfacePrivate *pointerPrivate = PointerInterfacePrivate::get(pointer);
        return pointerPrivate->relativePointersV1.get();
    }
    return nullptr;
}

void RelativePointerV1Interface::zwp_relative_pointer_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void RelativePointerV1Interface::sendRelativeMotion(const QPointF &delta, const QPointF &deltaNonAccelerated, std::chrono::microseconds time)
{
    if (!pointer->focusedSurface()) {
        return;
    }

    const double scale = pointer->focusedSurface()->scaleOverride() * pointer->focusedSurface()->compositorToClientScale();

    ClientConnection *focusedClient = pointer->focusedSurface()->client();
    const QList<Resource *> pointerResources = resourceMap().values(focusedClient->client());
    for (Resource *pointerResource : pointerResources) {
        if (pointerResource->client() == focusedClient->client()) {
            send_relative_motion(pointerResource->handle,
                                 time.count() >> 32,
                                 time.count() & 0xffffffff,
                                 wl_fixed_from_double(delta.x() * scale),
                                 wl_fixed_from_double(delta.y() * scale),
                                 wl_fixed_from_double(deltaNonAccelerated.x()),
                                 wl_fixed_from_double(deltaNonAccelerated.y()));
        }
    }
}

} // namespace KWin

#include "moc_relativepointer_v1.cpp"
