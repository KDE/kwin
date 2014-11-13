/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef WAYLAND_SERVER_GLOBAL_P_H
#define WAYLAND_SERVER_GLOBAL_P_H

#include "global.h"

struct wl_client;
struct wl_interface;

namespace KWayland
{
namespace Server
{

class Global::Private
{
public:
    static constexpr quint32 version = 0;
    virtual ~Private();
    void create();

    Display *display = nullptr;
    wl_global *global = nullptr;

protected:
    Private(Display *d, const wl_interface *interface, quint32 version);
    virtual void bind(wl_client *client, uint32_t version, uint32_t id) = 0;

    static void bind(wl_client *client, void *data, uint32_t version, uint32_t id);

    const wl_interface *const m_interface;
    const quint32 m_version;
};

}
}

#endif
