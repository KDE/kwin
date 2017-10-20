/****************************************************************************
Copyright 2017  Martin Flöser <mgraesslin@kde.org>

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
****************************************************************************/
#ifndef KWAYLAND_SERVER_IDLEINHIBIT_P_H
#define KWAYLAND_SERVER_IDLEINHIBIT_P_H

#include "idleinhibit_interface.h"
#include "global_p.h"
#include "resource_p.h"

#include <wayland-idle-inhibit-unstable-v1-server-protocol.h>

namespace KWayland
{
namespace Server
{

class Q_DECL_HIDDEN IdleInhibitManagerUnstableV1Interface : public IdleInhibitManagerInterface
{
    Q_OBJECT
public:
    explicit IdleInhibitManagerUnstableV1Interface(Display *display, QObject *parent = nullptr);
    ~IdleInhibitManagerUnstableV1Interface() override;

private:
    class Private;
};

class Q_DECL_HIDDEN IdleInhibitManagerInterface::Private : public Global::Private
{
public:
    IdleInhibitManagerInterfaceVersion interfaceVersion;

protected:
    Private(IdleInhibitManagerInterface *q, Display *d, const wl_interface *interface, quint32 version, IdleInhibitManagerInterfaceVersion interfaceVersion);
    IdleInhibitManagerInterface *q;
};

class Q_DECL_HIDDEN IdleInhibitorInterface : public Resource
{
    Q_OBJECT
public:
    explicit IdleInhibitorInterface(IdleInhibitManagerInterface *c, wl_resource *parentResource);

    virtual ~IdleInhibitorInterface();

    /**
     * @returns The interface version used by this IdleInhibitorInterface
     **/
    IdleInhibitManagerInterfaceVersion interfaceVersion() const;

protected:
    class Private;

private:
    Private *d_func() const;
    friend class IdleInhibitManagerUnstableV1Interface;
};

class Q_DECL_HIDDEN IdleInhibitorInterface::Private : public Resource::Private
{
public:
    Private(IdleInhibitorInterface *q, IdleInhibitManagerInterface *m, wl_resource *parentResource);
    ~Private();

private:

    IdleInhibitorInterface *q_func() {
        return reinterpret_cast<IdleInhibitorInterface *>(q);
    }

    static const struct zwp_idle_inhibitor_v1_interface s_interface;
};


}
}

#endif

