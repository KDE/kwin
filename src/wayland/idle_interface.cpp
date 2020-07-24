/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "idle_interface_p.h"
#include "display.h"
#include "seat_interface.h"

namespace KWaylandServer
{

const quint32 IdleInterfacePrivate::s_version = 1;

IdleInterfacePrivate::IdleInterfacePrivate(IdleInterface *_q, Display *display)
    : QtWaylandServer::org_kde_kwin_idle(*display, s_version)
    , q(_q)
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

    IdleTimeoutInterface *idleTimeout = new IdleTimeoutInterface(s, q, idleTimoutResource);
    idleTimeouts << idleTimeout;

    QObject::connect(idleTimeout, &IdleTimeoutInterface::destroyed, q, [this, idleTimeout]() {
        idleTimeouts.removeOne(idleTimeout);
    });
    idleTimeout->setup(timeout);
}

IdleInterface::IdleInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new IdleInterfacePrivate(this, display))
{
}

IdleInterface::~IdleInterface() = default;

void IdleInterface::inhibit()
{
    d->inhibitCount++;
    if (d->inhibitCount == 1) {
        emit inhibitedChanged();
    }
}

void IdleInterface::uninhibit()
{
    d->inhibitCount--;
    if (d->inhibitCount == 0) {
        emit inhibitedChanged();
    }
}

bool IdleInterface::isInhibited() const
{
    return d->inhibitCount > 0;
}

void IdleInterface::simulateUserActivity()
{
    for (auto i : qAsConst(d->idleTimeouts)) {
        i->simulateUserActivity();
    }
}

IdleTimeoutInterface::IdleTimeoutInterface(SeatInterface *seat, IdleInterface *manager, wl_resource *resource)
    : QObject()
    , QtWaylandServer::org_kde_kwin_idle_timeout(resource)
    , seat(seat)
    , manager(manager)
{
    connect(seat, &SeatInterface::timestampChanged, this,
        [this] {
            simulateUserActivity();
        }
    );
    connect(manager, &IdleInterface::inhibitedChanged, this,
        [this, manager] {
            if (!timer) {
                // not yet configured
                return;
            }
            if (manager->isInhibited()) {
                if (!timer->isActive()) {
                    send_resumed();
                }
                timer->stop();
            } else {
                timer->start();
            }
        }
    );
}

IdleTimeoutInterface::~IdleTimeoutInterface() = default;

void IdleTimeoutInterface::org_kde_kwin_idle_timeout_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void IdleTimeoutInterface::org_kde_kwin_idle_timeout_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete this;
}

void IdleTimeoutInterface::org_kde_kwin_idle_timeout_simulate_user_activity(Resource *resource)
{
    Q_UNUSED(resource)
    simulateUserActivity();
}
void IdleTimeoutInterface::simulateUserActivity()
{
    if (!timer) {
        // not yet configured
        return;
    }
    if (manager->isInhibited()) {
        // ignored while inhibited
        return;
    }
    if (!timer->isActive()) {
        send_resumed();
    }
    timer->start();
}

void IdleTimeoutInterface::setup(quint32 timeout)
{
    if (timer) {
        return;
    }
    timer = new QTimer(this);
    timer->setSingleShot(true);
    // less than 5 sec is not idle by definition
    timer->setInterval(qMax(timeout, 5000u));
    QObject::connect(timer, &QTimer::timeout, this,
        [this] {
            send_idle();
        }
    );
    if (manager->isInhibited()) {
        // don't start if inhibited
        return;
    }
    timer->start();
}
}
