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

#include "global.h"
#include "resource.h"

struct wl_resource;

namespace KWayland
{
namespace Server
{

class Display;
class SurfaceInterface;
class SubSurfaceInterface;

/**
 * @todo Add documentation
 */
class KWAYLANDSERVER_EXPORT SubCompositorInterface : public Global
{
    Q_OBJECT
public:
    virtual ~SubCompositorInterface();

Q_SIGNALS:
    void subSurfaceCreated(KWayland::Server::SubSurfaceInterface*);

private:
    explicit SubCompositorInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
};

/**
 * @todo Add documentation
 */
class KWAYLANDSERVER_EXPORT SubSurfaceInterface : public Resource
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

    /**
     * Whether this SubSurfaceInterface is in synchronized mode.
     * A SubSurface is in synchronized mode if either {@link mode} is
     * @c Mode::Synchronized or if the parent surface is in synchronized
     * mode. If a SubSurfaceInterface is in synchronized mode all child
     * SubSurfaceInterfaces are also in synchronized mode ignoring the actual mode.
     * @returns Whether this SubSurfaceInterface is in synchronized mode.
     * @see mode
     * @since 5.22
     **/
    bool isSynchronized() const;

    // TODO: remove with ABI break (KF6)
    QPointer<SurfaceInterface> surface();
    /**
     * @returns The surface this SubSurfaceInterface was created on.
     * @since 5.22
     **/
    QPointer<SurfaceInterface> surface() const;
    // TODO: remove with ABI break (KF6)
    QPointer<SurfaceInterface> parentSurface();
    /**
     * @returns The parent surface for which this SubSurfaceInterface is a child
     * @since 5.22
     **/
    QPointer<SurfaceInterface> parentSurface() const;

    /**
     * @returns the main surface for the sub-surface tree, that is the first surface without a parent
     * @since 5.22
     **/
    QPointer<SurfaceInterface> mainSurface() const;

Q_SIGNALS:
    void positionChanged(const QPoint&);
    void modeChanged(KWayland::Server::SubSurfaceInterface::Mode);

private:
    friend class SubCompositorInterface;
    friend class SurfaceInterface;
    explicit SubSurfaceInterface(SubCompositorInterface *parent, wl_resource *parentResource);

    class Private;
    Private *d_func() const;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::SubSurfaceInterface::Mode)

#endif
