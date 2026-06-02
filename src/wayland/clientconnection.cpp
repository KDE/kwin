/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "clientconnection.h"

#include "config-kwin.h"

#include "display.h"
#include "utils/executable_path.h"
// Qt
#include <QFileInfo>
#include <QList>
// Wayland
#include <wayland-server.h>

#if HAVE_SYSTEMD
#include <systemd/sd-login.h>
#endif

namespace KWin
{

#if HAVE_SYSTEMD
static bool startsWith(const char *string, const char *prefix)
{
    return strncmp(string, prefix, strlen(prefix)) == 0;
}

static bool isSandboxed(pid_t pid)
{
    char *userSlice = nullptr;
    const auto freeSlice = qScopeGuard([&userSlice]() {
        free(userSlice);
    });

    if (sd_pid_get_user_slice(pid, &userSlice) < 0) {
        return false;
    }

    if (strcmp(userSlice, "app.slice") != 0) {
        return false;
    }

    char *userUnit = nullptr;
    const auto freeUnit = qScopeGuard([&userUnit]() {
        free(userUnit);
    });

    if (sd_pid_get_user_unit(pid, &userUnit) < 0) {
        return false;
    }

    return startsWith(userUnit, "app-flatpak-")
        || startsWith(userUnit, "snap.");
}
#endif

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
    bool sandboxed = false;
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

#if HAVE_SYSTEMD
    sandboxed = isSandboxed(pid);
#endif
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

bool ClientConnection::isSandboxed() const
{
    return d->sandboxed;
}

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
    d->sandboxed = true;
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
