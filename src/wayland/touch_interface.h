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
#ifndef WAYLAND_SERVER_TOUCH_INTERFACE_H
#define WAYLAND_SERVER_TOUCH_INTERFACE_H

#include <KWayland/Server/kwaylandserver_export.h>

#include "resource.h"

namespace KWayland
{
namespace Server
{

class SeatInterface;

/**
 * @brief Resource for the wl_touch interface.
 *
 **/
class KWAYLANDSERVER_EXPORT TouchInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~TouchInterface();

private:
    void down(qint32 id, quint32 serial, const QPointF &localPos);
    void up(qint32 id, quint32 serial);
    void frame();
    void cancel();
    void move(qint32 id, const QPointF &localPos);
    friend class SeatInterface;
    explicit TouchInterface(SeatInterface *parent, wl_resource *parentResource);
    class Private;
    Private *d_func() const;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::TouchInterface*)

#endif
