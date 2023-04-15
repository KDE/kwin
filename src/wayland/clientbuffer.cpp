/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "wayland/clientbuffer.h"
#include "wayland/linuxdmabufv1clientbuffer.h"
#include "wayland/shmclientbuffer.h"

namespace KWaylandServer
{

ClientBuffer *ClientBuffer::get(wl_resource *resource)
{
    if (auto buffer = LinuxDmaBufV1ClientBuffer::get(resource)) {
        return buffer;
    }
    if (auto buffer = ShmClientBuffer::get(resource)) {
        return buffer;
    }
    return nullptr;
}

} // namespace KWaylandServer
