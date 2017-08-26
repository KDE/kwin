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
#include "global.h"
#include "global_p.h"
#include "display.h"
// wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

Global::Private::Private(Display *d, const wl_interface *interface, quint32 version)
    : display(d)
    , m_interface(interface)
    , m_version(version)
{
}

Global::Private::~Private() = default;

void Global::Private::bind(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    auto d = reinterpret_cast<Private*>(data);
    d->bind(client, version, id);
}

void Global::Private::create()
{
    Q_ASSERT(!global);
    global = wl_global_create(*display, m_interface, m_version, this, bind);
}

Global::Global(Global::Private *d, QObject *parent)
    : QObject(parent)
    , d(d)
{
}

Global::~Global()
{
    destroy();
}

void Global::create()
{
    d->create();
}

void Global::destroy()
{
    if (!d->global) {
        return;
    }
    emit aboutToDestroyGlobal();
    wl_global_destroy(d->global);
    d->global = nullptr;
}

bool Global::isValid() const
{
    return d->global != nullptr;
}

Global::operator wl_global*() const
{
    return d->global;
}

Global::operator wl_global*()
{
    return d->global;
}

Display *Global::display()
{
    return d->display;
}

}
}
