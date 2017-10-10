/********************************************************************
Copyright 2017  David Edmundson <davidedmundson@kde.org>

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

#include "filtered_display.h"
#include "display.h"

#include <wayland-server.h>

#include <QByteArray>

namespace KWayland
{
namespace Server
{

class FilteredDisplay::Private
{
public:
    Private(FilteredDisplay *_q);
    FilteredDisplay *q;
    static bool globalFilterCallback(const wl_client *client, const wl_global *global, void *data)
    {
        auto t = static_cast<FilteredDisplay::Private*>(data);
        auto clientConnection = t->q->getConnection(const_cast<wl_client*>(client));
        auto interface = wl_global_get_interface(global);
        auto name = QByteArray::fromRawData(interface->name, strlen(interface->name));
        return t->q->allowInterface(clientConnection, name);
    };
};

FilteredDisplay::Private::Private(FilteredDisplay *_q):
    q(_q)
{}


FilteredDisplay::FilteredDisplay(QObject *parent):
    Display(parent),
    d(new Private(this))
{
    connect(this, &Display::runningChanged, [this](bool running) {
        if (!running) {
            return;
        }
        wl_display_set_global_filter(*this, Private::globalFilterCallback, d.data());
    });
}

FilteredDisplay::~FilteredDisplay()
{
}

}
}
