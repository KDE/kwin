/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
    static void releaseCallback(wl_client *client, wl_resource *resource);
    static const struct org_kde_kwin_dpms_interface s_interface;
};

}
}

#endif
