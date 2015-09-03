/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef WAYLAND_SERVER_PLASMA_SHELL_INTERFACE_H
#define WAYLAND_SERVER_PLASMA_SHELL_INTERFACE_H

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
class PlasmaShellSurfaceInterface;

class KWAYLANDSERVER_EXPORT PlasmaShellInterface : public Global
{
    Q_OBJECT
public:
    virtual ~PlasmaShellInterface();

Q_SIGNALS:
    void surfaceCreated(KWayland::Server::PlasmaShellSurfaceInterface*);

private:
    friend class Display;
    explicit PlasmaShellInterface(Display *display, QObject *parent);
    class Private;
};

class KWAYLANDSERVER_EXPORT PlasmaShellSurfaceInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~PlasmaShellSurfaceInterface();

    SurfaceInterface *surface() const;
    PlasmaShellInterface *shell() const;

    QPoint position() const;
    bool isPositionSet() const;

    enum class Role {
        Normal,
        Desktop,
        Panel,
        OnScreenDisplay
    };
    Role role() const;
    enum class PanelBehavior {
        AlwaysVisible,
        AutoHide,
        WindowsCanCover,
        WindowsGoBelow
    };
    PanelBehavior panelBehavior() const;

Q_SIGNALS:
    void positionChanged();
    void roleChanged();
    void panelBehaviorChanged();

private:
    friend class PlasmaShellInterface;
    explicit PlasmaShellSurfaceInterface(PlasmaShellInterface *shell, SurfaceInterface *parent, wl_resource *parentResource);
    class Private;
    Private *d_func() const;
};

}
}

#endif
