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
#include "buffer_interface.h"
#include "surface_interface.h"

namespace KWayland
{
namespace Server
{

BufferInterface::BufferInterface(wl_resource *resource, SurfaceInterface *parent)
    : QObject(parent)
    , m_buffer(resource)
    , m_shmBuffer(wl_shm_buffer_get(m_buffer))
    , m_surface(parent)
    , m_refCount(0)
{
}

BufferInterface::~BufferInterface()
{
    Q_ASSERT(m_refCount == 0);
    releaseImage();
}

void BufferInterface::releaseImage()
{
    if (m_image.isNull()) {
        return;
    }
    // first destroy it
    m_image = QImage();
    wl_shm_buffer_end_access(m_shmBuffer);
}

void BufferInterface::ref()
{
    m_refCount++;
}

void BufferInterface::unref()
{
    Q_ASSERT(m_refCount > 0);
    m_refCount--;
    if (m_refCount == 0) {
        releaseImage();
        wl_buffer_send_release(m_buffer);
        deleteLater();
    }
}

QImage::Format BufferInterface::format() const
{
    switch (wl_shm_buffer_get_format(m_shmBuffer)) {
    case WL_SHM_FORMAT_ARGB8888:
        return QImage::Format_ARGB32;
    case WL_SHM_FORMAT_XRGB8888:
        return QImage::Format_RGB32;
    default:
        return QImage::Format_Invalid;
    }
}

QImage BufferInterface::data()
{
    if (m_image.isNull()) {
        createImage();
    }
    return m_image;
}

void BufferInterface::createImage()
{
    if (!m_shmBuffer) {
        return;
    }
    const QImage::Format imageFormat = format();
    if (imageFormat == QImage::Format_Invalid) {
        return;
    }
    wl_shm_buffer_begin_access(m_shmBuffer);
    m_image = QImage((const uchar*)wl_shm_buffer_get_data(m_shmBuffer),
                     wl_shm_buffer_get_width(m_shmBuffer),
                     wl_shm_buffer_get_height(m_shmBuffer),
                     wl_shm_buffer_get_stride(m_shmBuffer),
                     imageFormat);
}

}
}
