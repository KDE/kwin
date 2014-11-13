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
#ifndef WAYLAND_SERVER_SURFACE_INTERFACE_H
#define WAYLAND_SERVER_SURFACE_INTERFACE_H

#include "output_interface.h"

#include <QObject>
#include <QPointer>
#include <QRegion>

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{
class BufferInterface;
class CompositorInterface;
class SubSurfaceInterface;

class KWAYLANDSERVER_EXPORT SurfaceInterface : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QRegion damage READ damage NOTIFY damaged)
    Q_PROPERTY(QRegion opaque READ opaque NOTIFY opaqueChanged)
    Q_PROPERTY(QRegion input READ input NOTIFY inputChanged)
    Q_PROPERTY(qint32 scale READ scale NOTIFY scaleChanged)
    Q_PROPERTY(KWayland::Server::OutputInterface::Transform transform READ transform NOTIFY transformChanged)
public:
    virtual ~SurfaceInterface();

    void create(wl_client *client, quint32 version, quint32 id);

    void frameRendered(quint32 msec);

    wl_resource *resource() const;
    wl_client *client() const;

    QRegion damage() const;
    QRegion opaque() const;
    QRegion input() const;
    bool inputIsInfitine() const;
    qint32 scale() const;
    OutputInterface::Transform transform() const;
    BufferInterface *buffer();
    QPoint offset() const;

    /**
     * @returns The SubSurface for this Surface in case there is one.
     **/
    QPointer<SubSurfaceInterface> subSurface() const;
    /**
     * @returns Children in stacking order from bottom (first) to top (last).
     **/
    QList<QPointer<SubSurfaceInterface>> childSubSurfaces() const;

    static SurfaceInterface *get(wl_resource *native);

Q_SIGNALS:
    void damaged(const QRegion&);
    void opaqueChanged(const QRegion&);
    void inputChanged(const QRegion&);
    void scaleChanged(qint32);
    void transformChanged(KWayland::Server::OutputInterface::Transform);

private:
    friend class CompositorInterface;
    friend class SubSurfaceInterface;
    explicit SurfaceInterface(CompositorInterface *parent);

    class Private;
    QScopedPointer<Private> d;
};

}
}

#endif
