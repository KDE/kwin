/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

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
    IdleInterfacePrivate(Display *display);

protected:
    void org_kde_kwin_idle_get_idle_timeout(Resource *resource, uint32_t id, wl_resource *seat, uint32_t timeout) override;
};

class IdleTimeoutInterface : public QObject, public QtWaylandServer::org_kde_kwin_idle_timeout
{
    Q_OBJECT

public:
    explicit IdleTimeoutInterface(std::chrono::milliseconds timeout, wl_resource *resource);

protected:
    void org_kde_kwin_idle_timeout_destroy_resource(Resource *resource) override;
    void org_kde_kwin_idle_timeout_release(Resource *resource) override;
    void org_kde_kwin_idle_timeout_simulate_user_activity(Resource *resource) override;
};

}
