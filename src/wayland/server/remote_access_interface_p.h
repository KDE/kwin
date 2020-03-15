/*
    SPDX-FileCopyrightText: 2016 Oleg Chernovskiy <kanedias@xaker.ru>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_REMOTE_ACCESS_INTERFACE_P_H
#define KWAYLAND_SERVER_REMOTE_ACCESS_INTERFACE_P_H

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
