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
#ifndef WAYLAND_SERVER_POINTER_INTERFACE_H
#define WAYLAND_SERVER_POINTER_INTERFACE_H

#include <KWayland/Server/kwaylandserver_export.h>

#include "resource.h"

namespace KWayland
{
namespace Server
{

class SeatInterface;
class SurfaceInterface;

class KWAYLANDSERVER_EXPORT PointerInterface : public Resource
{
    Q_OBJECT
    Q_PROPERTY(QPointF globalPos READ globalPos WRITE setGlobalPos NOTIFY globalPosChanged)
public:
    virtual ~PointerInterface();

    /**
     * Convenient method to set the pointer position of the SeatInterface.
     * @see SeatInterface::setPointerPos
     **/
    void setGlobalPos(const QPointF &pos);
    /**
     * Convenient method to get the pointer position of the SeatInterface.
     * @see SeatInterface::pointerPos
     **/
    QPointF globalPos() const;
    void buttonPressed(quint32 button);
    void buttonPressed(Qt::MouseButton button);
    void buttonReleased(quint32 button);
    void buttonReleased(Qt::MouseButton button);
    bool isButtonPressed(quint32 button) const;
    bool isButtonPressed(Qt::MouseButton button) const;
    quint32 buttonSerial(quint32 button) const;
    quint32 buttonSerial(Qt::MouseButton button) const;
    void axis(Qt::Orientation orientation, quint32 delta);

Q_SIGNALS:
    /**
     * Convenient signal emitted when the SeatInterface's pointer position changes.
     * @see SeatInterface::pointerPosChanged
     **/
    void globalPosChanged(const QPointF &pos);

private:
    void setFocusedSurface(SurfaceInterface *surface, quint32 serial);
    SurfaceInterface *focusedSurface() const;
    friend class SeatInterface;
    explicit PointerInterface(SeatInterface *parent, wl_resource *parentResource);
    class Private;
    Private *d_func() const;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::PointerInterface*)

#endif
