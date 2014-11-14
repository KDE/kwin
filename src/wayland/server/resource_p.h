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
#ifndef WAYLAND_SERVER_RESOURCE_P_H
#define WAYLAND_SERVER_RESOURCE_P_H

#include "resource.h"
#include <wayland-server.h>
#include <type_traits>

namespace KWayland
{
namespace Server
{

class Resource::Private
{
public:
    virtual ~Private();
    virtual void create(wl_client *client, quint32 version, quint32 id) = 0;

    wl_resource *resource = nullptr;
    wl_client *client = nullptr;
    Global *global;

protected:
    explicit Private(Global *g);

    template <typename Derived>
    static Derived *cast(wl_resource *r) {
        static_assert(std::is_base_of<Private, Derived>::value,
                      "Derived must be derived from Resource::Private");
        return r ? reinterpret_cast<Derived*>(wl_resource_get_user_data(r)) : nullptr;
    }

};

}
}
#endif
