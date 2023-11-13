/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "clientconnection.h"
#include "display.h"
#include "utils/executable_path.h"
// Qt
#include <QFileInfo>
#include <QList>
// Wayland
#include <wayland-server.h>

namespace KWin
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
    QString securityContextAppId;
    qreal scaleOverride = 1.0;

private:
    static void destroyListenerCallback(wl_listener *listener, void *data);
    static void destroyLateListenerCallback(wl_listener *listener, void *data);
    ClientConnection *q;
    wl_listener destroyListener;
    wl_listener destroyLateListener;
    static QList<ClientConnectionPrivate *> s_allClients;
};

QList<ClientConnectionPrivate *> ClientConnectionPrivate::s_allClients;

ClientConnectionPrivate::ClientConnectionPrivate(wl_client *c, Display *display, ClientConnection *q)
    : client(c)
    , display(display)
    , q(q)
{
    s_allClients << this;

    destroyListener.notify = destroyListenerCallback;
    wl_client_add_destroy_listener(c, &destroyListener);

    destroyLateListener.notify = destroyLateListenerCallback;
    wl_client_add_destroy_late_listener(c, &destroyLateListener);

    wl_client_get_credentials(client, &pid, &user, &group);
    executablePath = executablePathFromPid(pid);
}

ClientConnectionPrivate::~ClientConnectionPrivate()
{
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
    wl_list_remove(&p->destroyListener.link);
    Q_EMIT q->disconnected(q);
}

void ClientConnectionPrivate::destroyLateListenerCallback(wl_listener *listener, void *data)
{
    wl_client *client = reinterpret_cast<wl_client *>(data);
    auto it = std::find_if(s_allClients.constBegin(), s_allClients.constEnd(), [client](ClientConnectionPrivate *c) {
        return c->client == client;
    });
    Q_ASSERT(it != s_allClients.constEnd());
    auto p = (*it);
    auto q = p->q;

    wl_list_remove(&p->destroyLateListener.link);
    delete q;
}

ClientConnection::ClientConnection(wl_client *c, Display *parent)
    : QObject(parent)
    , d(new ClientConnectionPrivate(c, parent, this))
{
}

ClientConnection::~ClientConnection() = default;

void ClientConnection::flush()
{
    wl_client_flush(d->client);
}

void ClientConnection::destroy()
{
    wl_client_destroy(d->client);
}

wl_resource *ClientConnection::getResource(quint32 id) const
{
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
    Q_ASSERT(scaleOveride != 0);
    d->scaleOverride = scaleOveride;
    Q_EMIT scaleOverrideChanged();
}

qreal ClientConnection::scaleOverride() const
{
    return d->scaleOverride;
}

void ClientConnection::setSecurityContextAppId(const QString &appId)
{
    d->securityContextAppId = appId;
}

QString ClientConnection::securityContextAppId() const
{
    return d->securityContextAppId;
}
}

#include "moc_clientconnection.cpp"
