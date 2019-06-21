/********************************************************************
Copyright 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

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

#ifndef KWAYLAND_KEYSTATE_INTERFACE_H
#define KWAYLAND_KEYSTATE_INTERFACE_H

#include <KWayland/Server/kwaylandserver_export.h>
#include "global.h"
#include "resource.h"

namespace KWayland
{
namespace Server
{

class Display;

/**
 * @brief Exposes key states to wayland clients
 *
 * @since 5.58
 **/
class KWAYLANDSERVER_EXPORT KeyStateInterface : public Global
{
    Q_OBJECT
public:
    virtual ~KeyStateInterface();

    enum class Key {
        CapsLock = 0,
        NumLock = 1,
        ScrollLock = 2,
    };
    Q_ENUM(Key);
    enum State {
        Unlocked = 0,
        Latched = 1,
        Locked = 2,
    };
    Q_ENUM(State)

    void setState(Key k, State s);

private:
    explicit KeyStateInterface(Display *display, QObject *parent = nullptr);
    friend class Display;

    class Private;
};

}
}

#endif
