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
#ifndef WAYLAND_SERVER_SHELL_INTERFACE_H
#define WAYLAND_SERVER_SHELL_INTERFACE_H

#include <QObject>

#include <KWayland/Server/kwaylandserver_export.h>

#include "global.h"
#include "resource.h"

class QSize;
struct wl_resource;

namespace KWayland
{
namespace Server
{

class Display;
class SurfaceInterface;
class ShellSurfaceInterface;

class KWAYLANDSERVER_EXPORT ShellInterface : public Global
{
    Q_OBJECT
public:
    virtual ~ShellInterface();

Q_SIGNALS:
    void surfaceCreated(KWayland::Server::ShellSurfaceInterface*);

private:
    friend class Display;
    explicit ShellInterface(Display *display, QObject *parent);
    class Private;
};

class KWAYLANDSERVER_EXPORT ShellSurfaceInterface : public Resource
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

    SurfaceInterface *surface() const;
    ShellInterface *shell() const;

    QString title() const;
    QByteArray windowClass() const;
    bool isFullscreen() const;
    bool isToplevel() const;

Q_SIGNALS:
    void titleChanged(const QString&);
    void windowClassChanged(const QByteArray&);
    void pingTimeout();
    void pongReceived();
    void fullscreenChanged(bool);
    void toplevelChanged(bool);

private:
    friend class ShellInterface;
    explicit ShellSurfaceInterface(ShellInterface *shell, SurfaceInterface *parent, wl_resource *parentResource);
    class Private;
    Private *d_func() const;
};

}
}

#endif
