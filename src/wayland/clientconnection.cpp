/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "clientconnection.h"
#include "display.h"
#include "utils/executable_path.h"
// Qt
#include <QFileInfo>
#include <QVector>
// Wayland
#include <wayland-server.h>

namespace KWaylandServer
{
class ClientConnectionPrivate
{
public:
    ClientConnectionPrivate(wl_client *c, Display *display, ClientConnection *q);
    ~ClientConnectionPrivate();

    wl_client *client;
    Display *display;
    pid_t pid = 0;
    uid_t user = 0;
    gid_t group = 0;
    QString executablePath;

    qreal scaleOverride = 1.0;

private:
    static void destroyListenerCallback(wl_listener *listener, void *data);
    ClientConnection *q;
    wl_listener listener;
    static QVector<ClientConnectionPrivate *> s_allClients;
};

QVector<ClientConnectionPrivate *> ClientConnectionPrivate::s_allClients;

ClientConnectionPrivate::ClientConnectionPrivate(wl_client *c, Display *display, ClientConnection *q)
    : client(c)
    , display(display)
    , q(q)
{
    s_allClients << this;
    listener.notify = destroyListenerCallback;
    wl_client_add_destroy_listener(c, &listener);
    wl_client_get_credentials(client, &pid, &user, &group);
    executablePath = executablePathFromPid(pid);
}

ClientConnectionPrivate::~ClientConnectionPrivate()
{
    if (client) {
        wl_list_remove(&listener.link);
    }
    s_allClients.removeAt(s_allClients.indexOf(this));
}

void ClientConnectionPrivate::destroyListenerCallback(wl_listener *listener, void *data)
{
    wl_client *client = reinterpret_cast<wl_client *>(data);
    auto it = std::find_if(s_allClients.constBegin(), s_allClients.constEnd(), [client](ClientConnectionPrivate *c) {
        return c->client == client;
    });
    Q_ASSERT(it != s_allClients.constEnd());
    auto p = (*it);
    auto q = p->q;
    Q_EMIT q->aboutToBeDestroyed();
    p->client = nullptr;
    wl_list_remove(&p->listener.link);
    Q_EMIT q->disconnected(q);
    q->deleteLater();
}

ClientConnection::ClientConnection(wl_client *c, Display *parent)
    : QObject(parent)
    , d(new ClientConnectionPrivate(c, parent, this))
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

wl_resource *ClientConnection::getResource(quint32 id) const
{
    if (!d->client) {
        return nullptr;
    }
    return wl_client_get_object(d->client, id);
}

wl_client *ClientConnection::client() const
{
    return d->client;
}

ClientConnection::operator wl_client *()
{
    return d->client;
}

ClientConnection::operator wl_client *() const
{
    return d->client;
}

Display *ClientConnection::display() const
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

void ClientConnection::setScaleOverride(qreal scaleOveride)
{
    d->scaleOverride = scaleOveride;
    Q_EMIT scaleOverrideChanged();
}

qreal ClientConnection::scaleOverride() const
{
    return d->scaleOverride;
}
}
