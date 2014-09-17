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

namespace KWayland
{
namespace Server
{

static const quint32 s_version = 1;

const struct wl_shell_interface ShellInterface::s_interface = {
    ShellInterface::createSurfaceCallback
};

ShellInterface::ShellInterface(Display *display, QObject *parent)
    : QObject(parent)
    , m_display(display)
    , m_shell(nullptr)
{
}

ShellInterface::~ShellInterface()
{
    destroy();
}

void ShellInterface::create()
{
    Q_ASSERT(!m_shell);
    m_shell = wl_global_create(*m_display, &wl_shell_interface, s_version, this, &ShellInterface::bind);
}

void ShellInterface::destroy()
{
    if (!m_shell) {
        return;
    }
    wl_global_destroy(m_shell);
    m_shell = nullptr;
}

void ShellInterface::bind(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    reinterpret_cast<ShellInterface*>(data)->bind(client, version, id);
}

void ShellInterface::bind(wl_client *client, uint32_t version, uint32_t id)
{
    wl_resource *shell = wl_resource_create(client, &wl_shell_interface, qMin(version, s_version), id);
    if (!shell) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(shell, &ShellInterface::s_interface, this, nullptr);
}

void ShellInterface::createSurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface)
{
    ShellInterface *s = reinterpret_cast<ShellInterface*>(wl_resource_get_user_data(resource));
    s->createSurface(client, wl_resource_get_version(resource), id,
                     reinterpret_cast<SurfaceInterface*>(wl_resource_get_user_data(surface)));
}

void ShellInterface::createSurface(wl_client *client, uint32_t version, uint32_t id, SurfaceInterface *surface)
{
    auto it = std::find_if(m_surfaces.constBegin(), m_surfaces.constEnd(),
        [surface](ShellSurfaceInterface *s) {
            return surface == s->surface();
        }
    );
    if (it != m_surfaces.constBegin()) {
        wl_resource_post_error(surface->surface(), WL_DISPLAY_ERROR_INVALID_OBJECT, "ShellSurface already created");
        return;
    }
    ShellSurfaceInterface *shellSurface = new ShellSurfaceInterface(this, surface);
    m_surfaces << shellSurface;
    connect(shellSurface, &ShellSurfaceInterface::destroyed, this,
        [this, shellSurface] {
            m_surfaces.removeAll(shellSurface);
        }
    );
    shellSurface->create(client, version, id);
    emit surfaceCreated(shellSurface);
}

/*********************************
 * ShellSurfaceInterface
 *********************************/

const struct wl_shell_surface_interface ShellSurfaceInterface::s_interface = {
    ShellSurfaceInterface::pongCallback,
    ShellSurfaceInterface::moveCallback,
    ShellSurfaceInterface::resizeCallback,
    ShellSurfaceInterface::setToplevelCallback,
    ShellSurfaceInterface::setTransientCallback,
    ShellSurfaceInterface::setFullscreenCallback,
    ShellSurfaceInterface::setPopupCalback,
    ShellSurfaceInterface::setMaximizedCallback,
    ShellSurfaceInterface::setTitleCallback,
    ShellSurfaceInterface::setClassCallback
};

ShellSurfaceInterface::ShellSurfaceInterface(ShellInterface *shell, SurfaceInterface *parent)
    : QObject(parent)
    , m_surface(parent)
    , m_shell(shell)
    , m_shellSurface(nullptr)
    , m_client(nullptr)
    , m_clientPid(0)
    , m_clientUser(0)
    , m_clientGroup(0)
    , m_title()
    , m_windowClass(QByteArray())
    , m_pingTimer(new QTimer(this))
    , m_fullscreen(false)
    , m_toplevel(false)
{
    m_pingTimer->setSingleShot(true);
    m_pingTimer->setInterval(1000);
    connect(m_pingTimer, &QTimer::timeout, this, &ShellSurfaceInterface::pingTimeout);
    connect(this, &ShellSurfaceInterface::fullscreenChanged, this,
        [this] (bool fullscreen) {
            if (!fullscreen) {
                return;
            }
            setToplevel(false);
        }
    );
    connect(this, &ShellSurfaceInterface::toplevelChanged, this,
        [this] (bool toplevel) {
            if (!toplevel) {
                return;
            }
            setFullscreen(false);
        }
    );
}

ShellSurfaceInterface::~ShellSurfaceInterface()
{
    if (m_shellSurface) {
        wl_resource_destroy(m_shellSurface);
    }
}

void ShellSurfaceInterface::create(wl_client *client, quint32 version, quint32 id)
{
    Q_ASSERT(!m_client);
    Q_ASSERT(!m_shellSurface);
    m_shellSurface = wl_resource_create(client, &wl_shell_surface_interface, version, id);
    if (!m_shellSurface) {
        wl_client_post_no_memory(client);
        return;
    }
    m_client = client;
    wl_client_get_credentials(m_client, &m_clientPid, &m_clientUser, &m_clientGroup);

    wl_resource_set_implementation(m_shellSurface, &ShellSurfaceInterface::s_interface, this, ShellSurfaceInterface::unbind);
}

void ShellSurfaceInterface::unbind(wl_resource *r)
{
    ShellSurfaceInterface *s = cast(r);
    s->m_shellSurface = nullptr;
    s->deleteLater();
}

void ShellSurfaceInterface::pongCallback(wl_client *client, wl_resource *resource, uint32_t serial)
{
    ShellSurfaceInterface *s = cast(resource);
    Q_ASSERT(client == s->m_client);
    s->pong(serial);
}

void ShellSurfaceInterface::pong(quint32 serial)
{
    if (m_pingTimer->isActive() && serial == m_pingSerial) {
        m_pingTimer->stop();
        emit pongReceived();
    }
}

void ShellSurfaceInterface::ping()
{
    if (m_pingTimer->isActive()) {
        return;
    }
    m_pingSerial = m_shell->display()->nextSerial();
    wl_shell_surface_send_ping(m_shellSurface, m_pingSerial);
    wl_client_flush(m_client);
    m_pingTimer->start();
}

void ShellSurfaceInterface::setPingTimeout(uint msec)
{
    m_pingTimer->setInterval(msec);
}

bool ShellSurfaceInterface::isPinged() const
{
    return m_pingTimer->isActive();
}

void ShellSurfaceInterface::requestSize(const QSize &size)
{
    // TODO: what about the edges?
    wl_shell_surface_send_configure(m_shellSurface, 0, size.width(), size.height());
    wl_client_flush(m_client);
}

void ShellSurfaceInterface::moveCallback(wl_client *client, wl_resource *resource, wl_resource *seat, uint32_t serial)
{
    Q_UNUSED(seat)
    Q_UNUSED(serial)
    ShellSurfaceInterface *s = cast(resource);
    Q_ASSERT(client == s->m_client);
    // TODO: implement
}

void ShellSurfaceInterface::resizeCallback(wl_client *client, wl_resource *resource, wl_resource *seat, uint32_t serial, uint32_t edges)
{
    Q_UNUSED(seat)
    Q_UNUSED(serial)
    Q_UNUSED(edges)
    ShellSurfaceInterface *s = cast(resource);
    Q_ASSERT(client == s->m_client);
    // TODO: implement
}

void ShellSurfaceInterface::setToplevelCallback(wl_client *client, wl_resource *resource)
{
    ShellSurfaceInterface *s = cast(resource);
    Q_ASSERT(client == s->m_client);
    s->setToplevel(true);
}

void ShellSurfaceInterface::setToplevel(bool toplevel)
{
    if (m_toplevel == toplevel) {
        return;
    }
    m_toplevel = toplevel;
    emit toplevelChanged(m_toplevel);
}

void ShellSurfaceInterface::setTransientCallback(wl_client *client, wl_resource *resource, wl_resource *parent,
                                                 int32_t x, int32_t y, uint32_t flags)
{
    Q_UNUSED(parent)
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(flags)
    ShellSurfaceInterface *s = cast(resource);
    Q_ASSERT(client == s->m_client);
    // TODO: implement
}

void ShellSurfaceInterface::setFullscreenCallback(wl_client *client, wl_resource *resource, uint32_t method,
                                                  uint32_t framerate, wl_resource *output)
{
    Q_UNUSED(method)
    Q_UNUSED(framerate)
    Q_UNUSED(output)
    ShellSurfaceInterface *s = cast(resource);
    Q_ASSERT(client == s->m_client);
    // TODO: add method, framerate and output
    s->setFullscreen(true);
}

void ShellSurfaceInterface::setFullscreen(bool fullscreen)
{
    if (m_fullscreen == fullscreen) {
        return;
    }
    m_fullscreen = fullscreen;
    emit fullscreenChanged(m_fullscreen);
}

void ShellSurfaceInterface::setPopupCalback(wl_client *client, wl_resource *resource, wl_resource *seat, uint32_t serial,
                                            wl_resource *parent, int32_t x, int32_t y, uint32_t flags)
{
    Q_UNUSED(seat)
    Q_UNUSED(serial)
    Q_UNUSED(parent)
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(flags)
    ShellSurfaceInterface *s = cast(resource);
    Q_ASSERT(client == s->m_client);
    // TODO: implement
}

void ShellSurfaceInterface::setMaximizedCallback(wl_client *client, wl_resource *resource, wl_resource *output)
{
    Q_UNUSED(output)
    ShellSurfaceInterface *s = cast(resource);
    Q_ASSERT(client == s->m_client);
    // TODO: implement
}

void ShellSurfaceInterface::setTitleCallback(wl_client *client, wl_resource *resource, const char *title)
{
    ShellSurfaceInterface *s = cast(resource);
    Q_ASSERT(client == s->m_client);
    s->setTitle(QString::fromUtf8(title));
}

void ShellSurfaceInterface::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }
    m_title = title;
    emit titleChanged(m_title);
}

void ShellSurfaceInterface::setClassCallback(wl_client *client, wl_resource *resource, const char *class_)
{
    ShellSurfaceInterface *s = cast(resource);
    Q_ASSERT(client == s->m_client);
    s->setWindowClass(QByteArray(class_));
}

void ShellSurfaceInterface::setWindowClass(const QByteArray &windowClass)
{
    if (m_windowClass == windowClass) {
        return;
    }
    m_windowClass = windowClass;
    emit windowClassChanged(m_windowClass);
}

}
}
