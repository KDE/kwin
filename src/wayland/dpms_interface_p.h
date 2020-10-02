/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_DPMS_INTERFACE_P_H
#define WAYLAND_SERVER_DPMS_INTERFACE_P_H

#include "dpms_interface.h"

#include <qwayland-server-dpms.h>

namespace KWaylandServer
{

class OutputInterface;

class DpmsManagerInterfacePrivate : public QtWaylandServer::org_kde_kwin_dpms_manager
{
public:
    DpmsManagerInterfacePrivate(DpmsManagerInterface *q, Display *d);

    DpmsManagerInterface *q;

protected:
    void org_kde_kwin_dpms_manager_get(Resource *resource, uint32_t id, wl_resource *output) override;
};

class DpmsInterface : public QObject, QtWaylandServer::org_kde_kwin_dpms
{
    Q_OBJECT
public:
    explicit DpmsInterface(OutputInterface *output, wl_resource *resource);
    ~DpmsInterface() override;

    void sendSupported();
    void sendMode();
    void sendDone();

    OutputInterface *output;

protected:
    void org_kde_kwin_dpms_destroy_resource(Resource *resource) override;
    void org_kde_kwin_dpms_set(Resource *resource, uint32_t mode) override;
    void org_kde_kwin_dpms_release(Resource *resource) override;

};

}

#endif
