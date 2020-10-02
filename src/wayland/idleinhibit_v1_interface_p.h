/*
    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_IDLEINHIBIT_INTERFACE_P_H
#define KWAYLAND_SERVER_IDLEINHIBIT_INTERFACE_P_H

#include "idleinhibit_v1_interface.h"

#include <qwayland-server-idle-inhibit-unstable-v1.h>

namespace KWaylandServer
{

class IdleInhibitManagerV1InterfacePrivate : public QtWaylandServer::zwp_idle_inhibit_manager_v1
{
public:
    IdleInhibitManagerV1InterfacePrivate(IdleInhibitManagerV1Interface *_q, Display *display);

    IdleInhibitManagerV1Interface *q;

protected:
    void zwp_idle_inhibit_manager_v1_destroy(Resource *resource) override;
    void zwp_idle_inhibit_manager_v1_create_inhibitor(Resource *resource, uint32_t id, wl_resource *surface) override;
};

class IdleInhibitorV1Interface : public QObject, QtWaylandServer::zwp_idle_inhibitor_v1
{
    Q_OBJECT
public:
    explicit IdleInhibitorV1Interface(wl_resource *resource);
    ~IdleInhibitorV1Interface() override;

protected:
    void zwp_idle_inhibitor_v1_destroy_resource(Resource *resource) override;
    void zwp_idle_inhibitor_v1_destroy(Resource *resource) override;
};

}

#endif

