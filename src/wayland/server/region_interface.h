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
#ifndef WAYLAND_SERVER_REGION_INTERFACE_H
#define WAYLAND_SERVER_REGION_INTERFACE_H

#include <QObject>
#include <QRegion>

#include <KWayland/Server/kwaylandserver_export.h>

#include "resource.h"

namespace KWayland
{
namespace Server
{
class CompositorInterface;

/**
 * @brief Resource for the wl_region.
 *
 * A RegionInterface gets created by the CompositorInterface and represents
 * a QRegion.
 *
 * @see CompositorInterface
 **/
class KWAYLANDSERVER_EXPORT RegionInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~RegionInterface();

    /**
     * @returns the data of the region as a QRegion.
     **/
    QRegion region() const;

    /**
     * @returns The RegionInterface for the @p native resource.
     **/
    static RegionInterface *get(wl_resource *native);

Q_SIGNALS:
    /**
     * Emitted whenever the region changes.
     **/
    void regionChanged(const QRegion&);

private:
    friend class CompositorInterface;
    explicit RegionInterface(CompositorInterface *parent, wl_resource *parentResource);

    class Private;
    Private *d_func() const;
};

}
}
Q_DECLARE_METATYPE(KWayland::Server::RegionInterface*)

#endif
