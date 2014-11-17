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
#include "clientconnection.h"
#include "display.h"
// Qt
#include <QVector>
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class ClientConnection::Private
{
public:
    explicit Private(wl_client *c, Display *display, ClientConnection *q);
    ~Private();

    wl_client *client;
    Display *display;
    pid_t pid = 0;
    uid_t user = 0;
    gid_t group = 0;

private:
    static void destroyListenerCallback(wl_listener *listener, void *data);
    ClientConnection *q;
    wl_listener listener;
    static QVector<Private*> s_allClients;
};

QVector<ClientConnection::Private*> ClientConnection::Private::s_allClients;

ClientConnection::Private::Private(wl_client *c, Display *display, ClientConnection *q)
    : client(c)
    , display(display)
    , q(q)
{
    s_allClients << this;
    listener.notify = destroyListenerCallback;
    wl_client_add_destroy_listener(c, &listener);
    wl_client_get_credentials(client, &pid, &user, &group);
}

ClientConnection::Private::~Private()
{
    wl_list_remove(&listener.link);
    s_allClients.removeAt(s_allClients.indexOf(this));
}

void ClientConnection::Private::destroyListenerCallback(wl_listener *listener, void *data)
{
    Q_UNUSED(listener)
    wl_client *client = reinterpret_cast<wl_client*>(data);
    auto it = std::find_if(s_allClients.constBegin(), s_allClients.constEnd(),
        [client](Private *c) {
            return c->client == client;
        }
    );
    Q_ASSERT(it != s_allClients.constEnd());
    auto q = (*it)->q;
    emit q->disconnected(q);
    delete q;
}

ClientConnection::ClientConnection(wl_client *c, Display *parent)
    : QObject(parent)
    , d(new Private(c, parent, this))
{
}

ClientConnection::~ClientConnection() = default;

wl_client *ClientConnection::client()
{
    return d->client;
}

ClientConnection::operator wl_client*()
{
    return d->client;
}

ClientConnection::operator wl_client*() const
{
    return d->client;
}

Display *ClientConnection::display()
{
    return d->display;
}

gid_t ClientConnection::groupId() const
{
    return d->group;
}

pid_t ClientConnection::processId() const
{
    return d->pid;
}

uid_t ClientConnection::userId() const
{
    return d->user;
}

}
}
