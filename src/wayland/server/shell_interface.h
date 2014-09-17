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
#ifndef KWIN_WAYLAND_SERVER_SHELL_INTERFACE_H
#define KWIN_WAYLAND_SERVER_SHELL_INTERFACE_H

#include <QObject>

#include <wayland-server.h>

#include <kwaylandserver_export.h>

class QSize;
class QTimer;
struct wl_global;

namespace KWayland
{
namespace Server
{

class Display;
class SurfaceInterface;
class ShellSurfaceInterface;

class KWAYLANDSERVER_EXPORT ShellInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~ShellInterface();

    void create();
    void destroy();

    bool isValid() const {
        return m_shell != nullptr;
    }

    Display *display() const {
        return m_display;
    }

Q_SIGNALS:
    void surfaceCreated(KWayland::Server::ShellSurfaceInterface*);

private:
    friend class Display;
    explicit ShellInterface(Display *display, QObject *parent);
    static void bind(wl_client *client, void *data, uint32_t version, uint32_t id);
    static void createSurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface);
    void bind(wl_client *client, uint32_t version, uint32_t id);
    void createSurface(wl_client *client, uint32_t version, uint32_t id, SurfaceInterface *surface);
    Display *m_display;
    wl_global *m_shell;
    static const struct wl_shell_interface s_interface;
    QList<ShellSurfaceInterface*> m_surfaces;
};

class KWAYLANDSERVER_EXPORT ShellSurfaceInterface : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QByteArray windowClass READ windowClass NOTIFY windowClassChanged)
    Q_PROPERTY(bool fullscreen READ isFullscreen NOTIFY fullscreenChanged)
    Q_PROPERTY(bool toplevel READ isToplevel NOTIFY toplevelChanged)
public:
    virtual ~ShellSurfaceInterface();

    void ping();
    void setPingTimeout(uint msec);
    bool isPinged() const;
    void requestSize(const QSize &size);

    SurfaceInterface *surface() const {
        return m_surface;
    }
    ShellInterface *shell() const {
        return m_shell;
    }
    wl_resource *shellSurface() const {
        return m_shellSurface;
    }

    const QString &title() const {
        return m_title;
    }
    const QByteArray &windowClass() const {
        return m_windowClass;
    }
    bool isFullscreen() const {
        return m_fullscreen;
    }
    bool isToplevel() const {
        return m_toplevel;
    }

    // TODO: keep them here or add a better encapsulation?
    pid_t clientPid() const {
        return m_clientPid;
    }
    uid_t clientUser() const {
        return m_clientUser;
    }
    gid_t clientGroup() const {
        return m_clientGroup;
    }

Q_SIGNALS:
    void titleChanged(const QString&);
    void windowClassChanged(const QByteArray&);
    void pingTimeout();
    void pongReceived();
    void fullscreenChanged(bool);
    void toplevelChanged(bool);

private:
    friend class ShellInterface;
    explicit ShellSurfaceInterface(ShellInterface *shell, SurfaceInterface *parent);
    void create(wl_client *client, quint32 version, quint32 id);
    void setTitle(const QString &title);
    void setWindowClass(const QByteArray &windowClass);
    void pong(quint32 serial);
    void setFullscreen(bool fullscreen);
    void setToplevel(bool toplevel);

    static ShellSurfaceInterface *cast(wl_resource *r) {
        return reinterpret_cast<ShellSurfaceInterface*>(wl_resource_get_user_data(r));
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

    SurfaceInterface *m_surface;
    ShellInterface *m_shell;
    wl_resource *m_shellSurface;
    wl_client *m_client;
    pid_t m_clientPid;
    uid_t m_clientUser;
    gid_t m_clientGroup;
    QString m_title;
    QByteArray m_windowClass;
    QTimer *m_pingTimer;
    quint32 m_pingSerial;
    bool m_fullscreen;
    bool m_toplevel;

    static const struct wl_shell_surface_interface s_interface;
};

}
}

#endif
