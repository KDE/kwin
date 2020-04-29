/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "clientconnection.h"
#include "display.h"
// Qt
#include <QFileInfo>
#include <QVector>
// Wayland
#include <wayland-server.h>

namespace KWaylandServer
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
    QString executablePath;

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
    executablePath = QFileInfo(QStringLiteral("/proc/%1/exe").arg(pid)).symLinkTarget();
}

ClientConnection::Private::~Private()
{
    if (client) {
        wl_list_remove(&listener.link);
    }
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
    auto p = (*it);
    auto q = p->q;
    p->client = nullptr;
    wl_list_remove(&p->listener.link);
    emit q->disconnected(q);
    q->deleteLater();
}

ClientConnection::ClientConnection(wl_client *c, Display *parent)
    : QObject(parent)
    , d(new Private(c, parent, this))
{
}

ClientConnection::~ClientConnection() = default;

void ClientConnection::flush()
{
    if (!d->client) {
        return;
    }
    wl_client_flush(d->client);
}

void ClientConnection::destroy()
{
    if (!d->client) {
        return;
    }
    wl_client_destroy(d->client);
}

wl_resource *ClientConnection::createResource(const wl_interface *interface, quint32 version, quint32 id)
{
    if (!d->client) {
        return nullptr;
    }
    return wl_resource_create(d->client, interface, version, id);
}

wl_resource *ClientConnection::getResource(quint32 id)
{
    if (!d->client) {
        return nullptr;
    }
    return wl_client_get_object(d->client, id);
}

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

QString ClientConnection::executablePath() const
{
    return d->executablePath;
}

}
