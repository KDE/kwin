/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_IDLE_INTERFACE_P_H
#define KWAYLAND_SERVER_IDLE_INTERFACE_P_H

#include "idle_interface.h"

#include <qwayland-server-idle.h>

#include <QTimer>


namespace KWaylandServer
{

class Display;
class SeatInterface;
class IdleTimeoutInterface;
class IdleTimeoutInterface;

class IdleInterfacePrivate : public QtWaylandServer::org_kde_kwin_idle
{
public:
    IdleInterfacePrivate(IdleInterface *_q, Display *display);

    int inhibitCount = 0;
    QVector<IdleTimeoutInterface *> idleTimeouts;
    IdleInterface *q;

protected:
    void org_kde_kwin_idle_get_idle_timeout(Resource *resource, uint32_t id, wl_resource *seat, uint32_t timeout) override;
};

class IdleTimeoutInterface : public QObject, QtWaylandServer::org_kde_kwin_idle_timeout
{
    Q_OBJECT
public:
    explicit IdleTimeoutInterface(SeatInterface *seat, IdleInterface *parent, wl_resource *resource);
    ~IdleTimeoutInterface() override;
    void setup(quint32 timeout);
    void simulateUserActivity();

private:
    SeatInterface *seat;
    IdleInterface *manager;
    QTimer *timer = nullptr;

protected:
    void org_kde_kwin_idle_timeout_destroy_resource(Resource *resource) override;
    void org_kde_kwin_idle_timeout_release(Resource *resource) override;
    void org_kde_kwin_idle_timeout_simulate_user_activity(Resource *resource) override;
};

}

#endif
