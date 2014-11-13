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
#ifndef WAYLAND_SERVER_SUBCOMPOSITOR_INTERFACE_H
#define WAYLAND_SERVER_SUBCOMPOSITOR_INTERFACE_H

#include <QObject>
#include <QPointer>

#include <KWayland/Server/kwaylandserver_export.h>

struct wl_resource;

namespace KWayland
{
namespace Server
{

class Display;
class SurfaceInterface;
class SubSurfaceInterface;

class KWAYLANDSERVER_EXPORT SubCompositorInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~SubCompositorInterface();

    void create();
    void destroy();
    bool isValid() const;

Q_SIGNALS:
    void subSurfaceCreated(KWayland::Server::SubSurfaceInterface*);

private:
    explicit SubCompositorInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
    QScopedPointer<Private> d;
};

class KWAYLANDSERVER_EXPORT SubSurfaceInterface : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QPoint position READ position NOTIFY positionChanged)
    Q_PROPERTY(KWayland::Server::SubSurfaceInterface::Mode mode READ mode NOTIFY modeChanged)
public:
    virtual ~SubSurfaceInterface();

    QPoint position() const;

    enum class Mode {
        Synchronized,
        Desynchronized
    };
    Mode mode() const;

    wl_resource *resource();
    QPointer<SurfaceInterface> surface();
    QPointer<SurfaceInterface> parentSurface();

Q_SIGNALS:
    void positionChanged(const QPoint&);
    void modeChanged(KWayland::Server::SubSurfaceInterface::Mode);

private:
    friend class SubCompositorInterface;
    friend class SurfaceInterface;
    explicit SubSurfaceInterface(SubCompositorInterface *parent);

    class Private;
    QScopedPointer<Private> d;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::SubSurfaceInterface::Mode)

#endif
