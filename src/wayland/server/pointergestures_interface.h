/****************************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

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
****************************************************************************/
#ifndef KWAYLAND_SERVER_POINTERGESTURES_INTERFACE_H
#define KWAYLAND_SERVER_POINTERGESTURES_INTERFACE_H

#include "global.h"

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{

/**
 * Enum describing the interface versions the PointerGesturesInterface can support.
 *
 * @since 5.29
 **/
enum class PointerGesturesInterfaceVersion {
    /**
     * zwp_pointer_gestures_v1, zwp_pointer_gesture_swipe_v1 and zwp_pointer_gesture_pinch_v1
     **/
    UnstableV1
};

/**
 * Manager object for the PointerGestures.
 *
 * Creates and manages pointer swipe and pointer pinch gestures which are
 * reported to the SeatInterface.
 *
 * @see Display::createPointerGestures
 * @since 5.29
 **/
class KWAYLANDSERVER_EXPORT PointerGesturesInterface : public Global
{
    Q_OBJECT
public:
    virtual ~PointerGesturesInterface();

    /**
     * @returns The interface version used by this PointerGesturesInterface
     **/
    PointerGesturesInterfaceVersion interfaceVersion() const;

protected:
    class Private;
    explicit PointerGesturesInterface(Private *d, QObject *parent = nullptr);

private:
    Private *d_func() const;
};

}
}

#endif
