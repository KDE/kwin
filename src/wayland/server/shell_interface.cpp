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
#include "shell_interface.h"
#include "display.h"
#include "surface_interface.h"

#include <QTimer>

#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

static const quint32 s_version = 1;

class ShellInterface::Private
{
public:
    Private(ShellInterface *q, Display *d);
    void create();

    Display *display;
    wl_global *shell = nullptr;
    QList<ShellSurfaceInterface*> surfaces;

private:
    static void bind(wl_client *client, void *data, uint32_t version, uint32_t id);
    static void createSurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface);
    void bind(wl_client *client, uint32_t version, uint32_t id);
    void createSurface(wl_client *client, uint32_t version, uint32_t id, SurfaceInterface *surface);

    ShellInterface *q;
    static const struct wl_shell_interface s_interface;
};

ShellInterface::Private::Private(ShellInterface *q, Display *d)
    : display(d)
    , q(q)
{
}

void ShellInterface::Private::create()
{
    Q_ASSERT(!shell);
    shell = wl_global_create(*display, &wl_shell_interface, s_version, this, &bind);
}

const struct wl_shell_interface ShellInterface::Private::s_interface = {
    createSurfaceCallback
};



class ShellSurfaceInterface::Private
{
public:
    Private(ShellSurfaceInterface *q, ShellInterface *shell, SurfaceInterface *surface);
    void create(wl_client *client, quint32 version, quint32 id);
    void setFullscreen(bool fullscreen);
    void setToplevel(bool toplevel);
    void ping();

    SurfaceInterface *surface;
    ShellInterface *shell;
    wl_resource *shellSurface = nullptr;
    wl_client *client = nullptr;
    pid_t clientPid = 0;
    uid_t clientUser = 0;
    gid_t clientGroup = 0;
    QString title;
    QByteArray windowClass;
    QScopedPointer<QTimer> pingTimer;
    quint32 pingSerial = 0;
    bool fullscreen = false;
    bool toplevel = false;

private:
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    static void unbind(wl_resource *r);
    // interface callbacks
    static void pongCallback(wl_client *client, wl_resource *resource, uint32_t serial);
    static void moveCallback(wl_client *client, wl_resource *resource, wl_resource *seat, uint32_t serial);
    static void resizeCallback(wl_client *client, wl_resource *resource, wl_resource *seat,
                               uint32_t serial, uint32_t edges);
    static void setToplevelCallback(wl_client *client, wl_resource *resource);
    static void setTransientCallback(wl_client *client, wl_resource *resource, wl_resource *parent,
                                     int32_t x, int32_t y, uint32_t flags);
    static void setFullscreenCallback(wl_client *client, wl_resource *resource, uint32_t method,
                                      uint32_t framerate, wl_resource *output);
    static void setPopupCalback(wl_client *client, wl_resource *resource, wl_resource *seat, uint32_t serial,
                                wl_resource *parent, int32_t x, int32_t y, uint32_t flags);
    static void setMaximizedCallback(wl_client *client, wl_resource *resource, wl_resource *output);
    static void setTitleCallback(wl_client *client, wl_resource *resource, const char *title);
    static void setClassCallback(wl_client *client, wl_resource *resource, const char *class_);

    void setTitle(const QString &title);
    void setWindowClass(const QByteArray &windowClass);
    void pong(quint32 serial);

    ShellSurfaceInterface *q;
    static const struct wl_shell_surface_interface s_interface;
};

ShellInterface::ShellInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new Private(this, display))
{
}

ShellInterface::~ShellInterface()
{
    destroy();
}

void ShellInterface::create()
{
    d->create();
}

void ShellInterface::destroy()
{
    if (!d->shell) {
        return;
    }
    wl_global_destroy(d->shell);
    d->shell = nullptr;
}

void ShellInterface::Private::bind(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    reinterpret_cast<ShellInterface::Private*>(data)->bind(client, version, id);
}

void ShellInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    wl_resource *shell = wl_resource_create(client, &wl_shell_interface, qMin(version, s_version), id);
    if (!shell) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(shell, &s_interface, this, nullptr);
}

void ShellInterface::Private::createSurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface)
{
    auto s = reinterpret_cast<ShellInterface::Private*>(wl_resource_get_user_data(resource));
    s->createSurface(client, wl_resource_get_version(resource), id, SurfaceInterface::get(surface));
}

void ShellInterface::Private::createSurface(wl_client *client, uint32_t version, uint32_t id, SurfaceInterface *surface)
{
    auto it = std::find_if(surfaces.constBegin(), surfaces.constEnd(),
        [surface](ShellSurfaceInterface *s) {
            return surface == s->surface();
        }
    );
    if (it != surfaces.constEnd()) {
        wl_resource_post_error(surface->surface(), WL_DISPLAY_ERROR_INVALID_OBJECT, "ShellSurface already created");
        return;
    }
    ShellSurfaceInterface *shellSurface = new ShellSurfaceInterface(q, surface);
    surfaces << shellSurface;
    QObject::connect(shellSurface, &ShellSurfaceInterface::destroyed, q,
        [this, shellSurface] {
            surfaces.removeAll(shellSurface);
        }
    );
    shellSurface->d->create(client, version, id);
    emit q->surfaceCreated(shellSurface);
}

bool ShellInterface::isValid() const
{
    return d->shell != nullptr;
}

Display *ShellInterface::display() const
{
    return d->display;
}

/*********************************
 * ShellSurfaceInterface
 *********************************/
ShellSurfaceInterface::Private::Private(ShellSurfaceInterface *q, ShellInterface *shell, SurfaceInterface *surface)
    : surface(surface)
    , shell(shell)
    , pingTimer(new QTimer)
    , q(q)
{
    pingTimer->setSingleShot(true);
    pingTimer->setInterval(1000);
}

const struct wl_shell_surface_interface ShellSurfaceInterface::Private::s_interface = {
    pongCallback,
    moveCallback,
    resizeCallback,
    setToplevelCallback,
    setTransientCallback,
    setFullscreenCallback,
    setPopupCalback,
    setMaximizedCallback,
    setTitleCallback,
    setClassCallback
};

ShellSurfaceInterface::ShellSurfaceInterface(ShellInterface *shell, SurfaceInterface *parent)
    : QObject(parent)
    , d(new Private(this, shell, parent))
{
    connect(d->pingTimer.data(), &QTimer::timeout, this, &ShellSurfaceInterface::pingTimeout);
    connect(this, &ShellSurfaceInterface::fullscreenChanged, this,
        [this] (bool fullscreen) {
            if (!fullscreen) {
                return;
            }
            d->setToplevel(false);
        }
    );
    connect(this, &ShellSurfaceInterface::toplevelChanged, this,
        [this] (bool toplevel) {
            if (!toplevel) {
                return;
            }
            d->setFullscreen(false);
        }
    );
}

ShellSurfaceInterface::~ShellSurfaceInterface()
{
    if (d->shellSurface) {
        wl_resource_destroy(d->shellSurface);
    }
}

void ShellSurfaceInterface::Private::create(wl_client *c, quint32 version, quint32 id)
{
    Q_ASSERT(!client);
    Q_ASSERT(!shellSurface);
    shellSurface = wl_resource_create(c, &wl_shell_surface_interface, version, id);
    if (!shellSurface) {
        wl_client_post_no_memory(c);
        return;
    }
    client = c;
    wl_client_get_credentials(client, &clientPid, &clientUser, &clientGroup);

    wl_resource_set_implementation(shellSurface, &s_interface, this, unbind);
}

void ShellSurfaceInterface::Private::unbind(wl_resource *r)
{
    auto s = cast(r);
    s->shellSurface = nullptr;
    s->q->deleteLater();
}

void ShellSurfaceInterface::Private::pongCallback(wl_client *client, wl_resource *resource, uint32_t serial)
{
    auto s = cast(resource);
    Q_ASSERT(client == s->client);
    s->pong(serial);
}

void ShellSurfaceInterface::Private::pong(quint32 serial)
{
    if (pingTimer->isActive() && serial == pingSerial) {
        pingTimer->stop();
        emit q->pongReceived();
    }
}

void ShellSurfaceInterface::ping()
{
    d->ping();
}

void ShellSurfaceInterface::Private::ping()
{
    if (pingTimer->isActive()) {
        return;
    }
    pingSerial = shell->display()->nextSerial();
    wl_shell_surface_send_ping(shellSurface, pingSerial);
    wl_client_flush(client);
    pingTimer->start();
}

void ShellSurfaceInterface::setPingTimeout(uint msec)
{
    d->pingTimer->setInterval(msec);
}

bool ShellSurfaceInterface::isPinged() const
{
    return d->pingTimer->isActive();
}

void ShellSurfaceInterface::requestSize(const QSize &size)
{
    // TODO: what about the edges?
    wl_shell_surface_send_configure(d->shellSurface, 0, size.width(), size.height());
    wl_client_flush(d->client);
}

void ShellSurfaceInterface::Private::moveCallback(wl_client *client, wl_resource *resource, wl_resource *seat, uint32_t serial)
{
    Q_UNUSED(seat)
    Q_UNUSED(serial)
    auto s = cast(resource);
    Q_ASSERT(client == s->client);
    // TODO: implement
}

void ShellSurfaceInterface::Private::resizeCallback(wl_client *client, wl_resource *resource, wl_resource *seat, uint32_t serial, uint32_t edges)
{
    Q_UNUSED(seat)
    Q_UNUSED(serial)
    Q_UNUSED(edges)
    auto s = cast(resource);
    Q_ASSERT(client == s->client);
    // TODO: implement
}

void ShellSurfaceInterface::Private::setToplevelCallback(wl_client *client, wl_resource *resource)
{
    auto s = cast(resource);
    Q_ASSERT(client == s->client);
    s->setToplevel(true);
}

void ShellSurfaceInterface::Private::setToplevel(bool t)
{
    if (toplevel == t) {
        return;
    }
    toplevel = t;
    emit q->toplevelChanged(toplevel);
}

void ShellSurfaceInterface::Private::setTransientCallback(wl_client *client, wl_resource *resource, wl_resource *parent,
                                                 int32_t x, int32_t y, uint32_t flags)
{
    Q_UNUSED(parent)
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(flags)
    auto s = cast(resource);
    Q_ASSERT(client == s->client);
    // TODO: implement
}

void ShellSurfaceInterface::Private::setFullscreenCallback(wl_client *client, wl_resource *resource, uint32_t method,
                                                  uint32_t framerate, wl_resource *output)
{
    Q_UNUSED(method)
    Q_UNUSED(framerate)
    Q_UNUSED(output)
    auto s = cast(resource);
    Q_ASSERT(client == s->client);
    // TODO: add method, framerate and output
    s->setFullscreen(true);
}

void ShellSurfaceInterface::Private::setFullscreen(bool f)
{
    if (fullscreen == f) {
        return;
    }
    fullscreen = f;
    emit q->fullscreenChanged(fullscreen);
}

void ShellSurfaceInterface::Private::setPopupCalback(wl_client *client, wl_resource *resource, wl_resource *seat, uint32_t serial,
                                            wl_resource *parent, int32_t x, int32_t y, uint32_t flags)
{
    Q_UNUSED(seat)
    Q_UNUSED(serial)
    Q_UNUSED(parent)
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(flags)
    auto s = cast(resource);
    Q_ASSERT(client == s->client);
    // TODO: implement
}

void ShellSurfaceInterface::Private::setMaximizedCallback(wl_client *client, wl_resource *resource, wl_resource *output)
{
    Q_UNUSED(output)
    auto s = cast(resource);
    Q_ASSERT(client == s->client);
    // TODO: implement
}

void ShellSurfaceInterface::Private::setTitleCallback(wl_client *client, wl_resource *resource, const char *title)
{
    auto s = cast(resource);
    Q_ASSERT(client == s->client);
    s->setTitle(QString::fromUtf8(title));
}

void ShellSurfaceInterface::Private::setTitle(const QString &t)
{
    if (title == t) {
        return;
    }
    title = t;
    emit q->titleChanged(title);
}

void ShellSurfaceInterface::Private::setClassCallback(wl_client *client, wl_resource *resource, const char *class_)
{
    auto s = cast(resource);
    Q_ASSERT(client == s->client);
    s->setWindowClass(QByteArray(class_));
}

void ShellSurfaceInterface::Private::setWindowClass(const QByteArray &wc)
{
    if (windowClass == wc) {
        return;
    }
    windowClass = wc;
    emit q->windowClassChanged(windowClass);
}

SurfaceInterface *ShellSurfaceInterface::surface() const {
    return d->surface;
}

ShellInterface *ShellSurfaceInterface::shell() const {
    return d->shell;
}

wl_resource *ShellSurfaceInterface::shellSurface() const {
    return d->shellSurface;
}

QString ShellSurfaceInterface::title() const {
    return d->title;
}

QByteArray ShellSurfaceInterface::windowClass() const {
    return d->windowClass;
}

bool ShellSurfaceInterface::isFullscreen() const {
    return d->fullscreen;
}

bool ShellSurfaceInterface::isToplevel() const {
    return d->toplevel;
}

pid_t ShellSurfaceInterface::clientPid() const {
    return d->clientPid;
}

uid_t ShellSurfaceInterface::clientUser() const {
    return d->clientUser;
}

gid_t ShellSurfaceInterface::clientGroup() const {
    return d->clientGroup;
}

}
}
