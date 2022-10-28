/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "display.h"
#include "idle_interface_p.h"
#include "seat_interface.h"

#include "idledetector.h"
#include "input.h"

using namespace KWin;

namespace KWaylandServer
{

static const quint32 s_version = 1;

IdleInterfacePrivate::IdleInterfacePrivate(Display *display)
    : QtWaylandServer::org_kde_kwin_idle(*display, s_version)
{
}

void IdleInterfacePrivate::org_kde_kwin_idle_get_idle_timeout(Resource *resource, uint32_t id, wl_resource *seat, uint32_t timeout)
{
    SeatInterface *s = SeatInterface::get(seat);
    Q_ASSERT(s);

    wl_resource *idleTimoutResource = wl_resource_create(resource->client(), &org_kde_kwin_idle_timeout_interface, resource->version(), id);
    if (!idleTimoutResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    new IdleTimeoutInterface(std::chrono::milliseconds(timeout), idleTimoutResource);
}

IdleInterface::IdleInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new IdleInterfacePrivate(display))
{
}

IdleInterface::~IdleInterface() = default;

IdleTimeoutInterface::IdleTimeoutInterface(std::chrono::milliseconds timeout, wl_resource *resource)
    : QtWaylandServer::org_kde_kwin_idle_timeout(resource)
{
    auto detector = new IdleDetector(timeout, this);
    connect(detector, &IdleDetector::idle, this, [this]() {
        send_idle();
    });
    connect(detector, &IdleDetector::resumed, this, [this]() {
        send_resumed();
    });
}

void IdleTimeoutInterface::org_kde_kwin_idle_timeout_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void IdleTimeoutInterface::org_kde_kwin_idle_timeout_destroy_resource(Resource *resource)
{
    delete this;
}

void IdleTimeoutInterface::org_kde_kwin_idle_timeout_simulate_user_activity(Resource *resource)
{
    input()->simulateUserActivity();
}

}
