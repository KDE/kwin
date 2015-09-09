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
#ifndef WAYLAND_SERVER_COMPOSITOR_INTERFACE_H
#define WAYLAND_SERVER_COMPOSITOR_INTERFACE_H

#include "global.h"
#include "region_interface.h"
#include "surface_interface.h"

#include <QObject>

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{

class Display;
class SurfaceInterface;

/**
 * @brief Represents the Global for wl_compositor interface.
 *
 **/
class KWAYLANDSERVER_EXPORT CompositorInterface : public Global
{
    Q_OBJECT
public:
    virtual ~CompositorInterface();

Q_SIGNALS:
    /**
     * Emitted whenever this CompositorInterface created a SurfaceInterface.
     **/
    void surfaceCreated(KWayland::Server::SurfaceInterface*);
    /**
     * Emitted whenever this CompositorInterface created a RegionInterface.
     **/
    void regionCreated(KWayland::Server::RegionInterface*);

private:
    explicit CompositorInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
};

}
}

#endif
