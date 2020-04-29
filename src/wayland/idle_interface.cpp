/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "idle_interface.h"
#include "display.h"
#include "global_p.h"
#include "resource_p.h"
#include "seat_interface.h"

#include <QTimer>

#include <functional>
#include <wayland-server.h>
#include <wayland-idle-server-protocol.h>

namespace KWaylandServer
{

class IdleInterface::Private : public Global::Private
{
public:
    Private(IdleInterface *q, Display *d);

    int inhibitCount = 0;
    QVector<IdleTimeoutInterface*> idleTimeouts;

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    static void getIdleTimeoutCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *seat, uint32_t timeout);

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    IdleInterface *q;
    static const struct org_kde_kwin_idle_interface s_interface;
    static const quint32 s_version;
};

class IdleTimeoutInterface::Private : public Resource::Private
{
public:
    Private(SeatInterface *seat, IdleTimeoutInterface *q, IdleInterface *manager, wl_resource *parentResource);
    ~Private();
    void setup(quint32 timeout);

    void simulateUserActivity();

    SeatInterface *seat;
    QTimer *timer = nullptr;

private:
    static void simulateUserActivityCallback(wl_client *client, wl_resource *resource);
    IdleTimeoutInterface *q_func() {
        return reinterpret_cast<IdleTimeoutInterface*>(q);
    }
    static const struct org_kde_kwin_idle_timeout_interface s_interface;
};

const quint32 IdleInterface::Private::s_version = 1;

#ifndef K_DOXYGEN
const struct org_kde_kwin_idle_interface IdleInterface::Private::s_interface = {
    getIdleTimeoutCallback
};
#endif

IdleInterface::Private::Private(IdleInterface *q, Display *d)
    : Global::Private(d, &org_kde_kwin_idle_interface, s_version)
    , q(q)
{
}

void IdleInterface::Private::getIdleTimeoutCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *seat, uint32_t timeout)
{
    Private *p = cast(resource);
    SeatInterface *s = SeatInterface::get(seat);
    Q_ASSERT(s);
    IdleTimeoutInterface *idleTimeout = new IdleTimeoutInterface(s, p->q, resource);
    idleTimeout->create(p->display->getConnection(client), wl_resource_get_version(resource), id);
    if (!idleTimeout->resource()) {
        wl_resource_post_no_memory(resource);
        delete idleTimeout;
        return;
    }
    p->idleTimeouts << idleTimeout;
    QObject::connect(idleTimeout, &IdleTimeoutInterface::aboutToBeUnbound, p->q, [p, idleTimeout]() {
        p->idleTimeouts.removeOne(idleTimeout);
    });
    idleTimeout->d_func()->setup(timeout);
}

void IdleInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&org_kde_kwin_idle_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    // TODO: should we track?
}

void IdleInterface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
}

IdleInterface::IdleInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

IdleInterface::~IdleInterface() = default;

void IdleInterface::inhibit()
{
    Q_D();
    d->inhibitCount++;
    if (d->inhibitCount == 1) {
        emit inhibitedChanged();
    }
}

void IdleInterface::uninhibit()
{
    Q_D();
    d->inhibitCount--;
    if (d->inhibitCount == 0) {
        emit inhibitedChanged();
    }
}

bool IdleInterface::isInhibited() const
{
    Q_D();
    return d->inhibitCount > 0;
}

void IdleInterface::simulateUserActivity()
{
    Q_D();
    for (auto i : qAsConst(d->idleTimeouts)) {
        i->d_func()->simulateUserActivity();
    }
}

IdleInterface::Private *IdleInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

#ifndef K_DOXYGEN
const struct org_kde_kwin_idle_timeout_interface IdleTimeoutInterface::Private::s_interface = {
    resourceDestroyedCallback,
    simulateUserActivityCallback
};
#endif

IdleTimeoutInterface::Private::Private(SeatInterface *seat, IdleTimeoutInterface *q, IdleInterface *manager, wl_resource *parentResource)
    : Resource::Private(q, manager, parentResource, &org_kde_kwin_idle_timeout_interface, &s_interface)
    , seat(seat)
{
}

IdleTimeoutInterface::Private::~Private() = default;

void IdleTimeoutInterface::Private::simulateUserActivityCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client);
    Private *p = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    p->simulateUserActivity();
}

void IdleTimeoutInterface::Private::simulateUserActivity()
{
    if (!timer) {
        // not yet configured
        return;
    }
    if (qobject_cast<IdleInterface*>(global)->isInhibited()) {
        // ignored while inhibited
        return;
    }
    if (!timer->isActive() && resource) {
        org_kde_kwin_idle_timeout_send_resumed(resource);
    }
    timer->start();
}

void IdleTimeoutInterface::Private::setup(quint32 timeout)
{
    if (timer) {
        return;
    }
    timer = new QTimer(q);
    timer->setSingleShot(true);
    // less than 5 sec is not idle by definition
    timer->setInterval(qMax(timeout, 5000u));
    QObject::connect(timer, &QTimer::timeout, q,
        [this] {
            if (resource) {
                org_kde_kwin_idle_timeout_send_idle(resource);
            }
        }
    );
    if (qobject_cast<IdleInterface*>(global)->isInhibited()) {
        // don't start if inhibited
        return;
    }
    timer->start();
}

IdleTimeoutInterface::IdleTimeoutInterface(SeatInterface *seat, IdleInterface *parent, wl_resource *parentResource)
    : Resource(new Private(seat, this, parent, parentResource))
{
    connect(seat, &SeatInterface::timestampChanged, this,
        [this] {
            Q_D();
            d->simulateUserActivity();
        }
    );
    connect(parent, &IdleInterface::inhibitedChanged, this,
        [this] {
            Q_D();
            if (!d->timer) {
                // not yet configured
                return;
            }
            if (qobject_cast<IdleInterface*>(d->global)->isInhibited()) {
                if (!d->timer->isActive() && d->resource) {
                    org_kde_kwin_idle_timeout_send_resumed(d->resource);
                }
                d->timer->stop();
            } else {
                d->timer->start();
            }
        }
    );
}

IdleTimeoutInterface::~IdleTimeoutInterface() = default;

IdleTimeoutInterface::Private *IdleTimeoutInterface::d_func() const
{
    return reinterpret_cast<IdleTimeoutInterface::Private*>(d.data());
}

}
