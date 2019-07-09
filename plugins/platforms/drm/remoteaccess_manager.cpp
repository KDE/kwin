/********************************************************************
 *
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Oleg Chernovskiy <kanedias@xaker.ru>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "drm_output.h"
#include "remoteaccess_manager.h"
#include "logging.h"
#include "drm_backend.h"
#include "../../../wayland_server.h"

// system
#include <KWayland/Server/output_interface.h>
#include <unistd.h>
#include <gbm.h>

#include <cerrno>

namespace KWin
{

RemoteAccessManager::RemoteAccessManager(QObject *parent)
    : QObject(parent)
{
    if (waylandServer()) {
        m_interface = waylandServer()->display()->createRemoteAccessManager(this);
        m_interface->create();

        connect(m_interface, &RemoteAccessManagerInterface::bufferReleased,
                this, &RemoteAccessManager::releaseBuffer);
    }
}

RemoteAccessManager::~RemoteAccessManager()
{
    if (m_interface) {
        m_interface->destroy();
    }
}

void RemoteAccessManager::releaseBuffer(const BufferHandle *buf)
{
    int ret = close(buf->fd());
    if (Q_UNLIKELY(ret)) {
        qCWarning(KWIN_DRM) << "Couldn't close released GBM fd:" << strerror(errno);
    }
    delete buf;
}

void RemoteAccessManager::passBuffer(DrmOutput *output, DrmBuffer *buffer)
{
    DrmSurfaceBuffer* gbmbuf = static_cast<DrmSurfaceBuffer *>(buffer);

    // no connected RemoteAccess instance
    if (!m_interface || !m_interface->isBound()) {
        return;
    }

    // first buffer may be null
    if (!gbmbuf || !gbmbuf->hasBo()) {
        return;
    }

    auto buf = new BufferHandle;
    auto bo = gbmbuf->getBo();
    buf->setFd(gbm_bo_get_fd(bo));
    buf->setSize(gbm_bo_get_width(bo), gbm_bo_get_height(bo));
    buf->setStride(gbm_bo_get_stride(bo));
    buf->setFormat(gbm_bo_get_format(bo));

    m_interface->sendBufferReady(output->waylandOutput().data(), buf);
}

} // KWin namespace
