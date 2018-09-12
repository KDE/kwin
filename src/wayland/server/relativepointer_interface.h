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
#ifndef KWAYLAND_SERVER_RELATIVE_POINTER_INTERFACE_H
#define KWAYLAND_SERVER_RELATIVE_POINTER_INTERFACE_H

#include "global.h"

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{

class Display;

enum class RelativePointerInterfaceVersion {
    /**
     * zwp_relative_pointer_manager_v1 and zwp_relative_pointer_v1
     **/
    UnstableV1
};

/**
 * Manager object to create relative pointer interfaces.
 *
 * Once created the interaction happens through the SeatInterface class
 * which automatically delegates relative motion events to the created relative pointer
 * interfaces.
 *
 * @see SeatInterface::relativePointerMotion
 * @since 5.28
 **/
class KWAYLANDSERVER_EXPORT RelativePointerManagerInterface : public Global
{
    Q_OBJECT
public:
    virtual ~RelativePointerManagerInterface();

    /**
     * @returns The interface version used by this RelativePointerManagerInterface
     **/
    RelativePointerInterfaceVersion interfaceVersion() const;

protected:
    class Private;
    explicit RelativePointerManagerInterface(Private *d, QObject *parent = nullptr);

private:
    Private *d_func() const;
};

}
}

#endif
