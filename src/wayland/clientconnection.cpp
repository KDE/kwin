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

    wl_client *client;
    Display *display;
    pid_t pid = 0;
    uid_t user = 0;
    gid_t group = 0;
    QString executablePath;
    QString securityContextAppId;
    qreal scaleOverride = 1.0;
    bool tearingDown = false;

private:
    static void destroyListenerCallback(wl_listener *listener, void *data);

    wl_listener destroyListener;
};

ClientConnectionPrivate::ClientConnectionPrivate(wl_client *c, Display *display, ClientConnection *q)
    : client(c)
    , display(display)
{
    wl_client_set_user_data(c, q, [](void *userData) {
        ClientConnection *q = static_cast<ClientConnection *>(userData);
        delete q;
    });

    destroyListener.notify = destroyListenerCallback;
    wl_client_add_destroy_listener(c, &destroyListener);

    wl_client_get_credentials(client, &pid, &user, &group);
    executablePath = executablePathFromPid(pid);
}

void ClientConnectionPrivate::destroyListenerCallback(wl_listener *listener, void *data)
{
    ClientConnection *q = ClientConnection::get(reinterpret_cast<wl_client *>(data));

    Q_EMIT q->aboutToBeDestroyed();
    wl_list_remove(&q->d->destroyListener.link);
    q->d->tearingDown = true;
}

ClientConnection::ClientConnection(wl_client *c, Display *parent)
    : QObject(parent)
    , d(new ClientConnectionPrivate(c, parent, this))
{
}

ClientConnection::~ClientConnection() = default;

bool ClientConnection::tearingDown() const
{
    return d->tearingDown;
}

void ClientConnection::flush()
{
    wl_client_flush(d->client);
}

void ClientConnection::destroy()
{
    wl_client_destroy(d->client);
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

ClientConnection *ClientConnection::get(wl_client *native)
{
    return static_cast<ClientConnection *>(wl_client_get_user_data(native));
}
}

#include "moc_clientconnection.cpp"
