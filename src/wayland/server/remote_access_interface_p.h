/****************************************************************************
Copyright 2016  Oleg Chernovskiy <kanedias@xaker.ru>

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
#ifndef KWAYLAND_SERVER_REMOTE_ACCESS_P_H
#define KWAYLAND_SERVER_REMOTE_ACCESS_P_H

#include "resource.h"

namespace KWayland
{
namespace Server
{
    
/**
 * @class RemoteBufferInterface
 * @brief Internally used class. Holds data of passed buffer and client resource. Also controls buffer lifecycle.
 * @see RemoteAccessManagerInterface
 */
class RemoteBufferInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~RemoteBufferInterface() = default;

    /**
     * Sends GBM fd to the client.
     * Note that server still has to close mirror fd from its side.
     **/
    void passFd();

private:
    explicit RemoteBufferInterface(RemoteAccessManagerInterface *ram, wl_resource *pResource, const BufferHandle *buf);
    friend class RemoteAccessManagerInterface;

    class Private;
    Private *d_func() const;
};

}
}

#endif // KWAYLAND_SERVER_REMOTE_ACCESS_P_H
