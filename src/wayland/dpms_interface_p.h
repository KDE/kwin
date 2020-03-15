/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_DPMS_INTERFACE_P_H
#define WAYLAND_SERVER_DPMS_INTERFACE_P_H

#include "dpms_interface.h"
#include "global_p.h"
#include "resource_p.h"

#include <wayland-dpms-server-protocol.h>

namespace KWayland
{
namespace Server
{

class OutputInterface;

class DpmsManagerInterface::Private : public Global::Private
{
public:
    Private(DpmsManagerInterface *q, Display *d);

private:
    static void getDpmsCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *output);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    DpmsManagerInterface *q;
    static const struct org_kde_kwin_dpms_manager_interface s_interface;
    static const quint32 s_version;
};

class DpmsInterface : public Resource
{
    Q_OBJECT
public:
    explicit DpmsInterface(OutputInterface *output, wl_resource *parentResource, DpmsManagerInterface *manager);
    virtual ~DpmsInterface();

    void sendSupported();
    void sendMode();
    void sendDone();

private:
    class Private;
    Private *d_func() const;
};

class DpmsInterface::Private : public Resource::Private
{
public:
    explicit Private(DpmsInterface *q, DpmsManagerInterface *g, wl_resource *parentResource, OutputInterface *output);

    OutputInterface *output;

private:
    static void setCallback(wl_client *client, wl_resource *resource, uint32_t mode);
    static const struct org_kde_kwin_dpms_interface s_interface;
};

}
}

#endif
