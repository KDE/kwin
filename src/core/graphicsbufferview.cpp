/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/graphicsbufferview.h"
#include "core/graphicsbuffer.h"
#include "utils/common.h"

#include <drm_fourcc.h>

namespace KWin
{

static QImage::Format drmFormatToQImageFormat(uint32_t drmFormat)
{
    switch (drmFormat) {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    case DRM_FORMAT_ABGR16161616:
        return QImage::Format_RGBA64_Premultiplied;
    case DRM_FORMAT_XBGR16161616:
        return QImage::Format_RGBX64;
    case DRM_FORMAT_ARGB2101010:
        return QImage::Format_A2RGB30_Premultiplied;
    case DRM_FORMAT_XRGB2101010:
        return QImage::Format_RGB30;
    case DRM_FORMAT_ABGR2101010:
        return QImage::Format_A2BGR30_Premultiplied;
    case DRM_FORMAT_XBGR2101010:
        return QImage::Format_BGR30;
#endif
    case DRM_FORMAT_ARGB8888:
        return QImage::Format_ARGB32_Premultiplied;
    case DRM_FORMAT_XRGB8888:
        return QImage::Format_RGB32;
    default:
        return QImage::Format_Invalid;
    }
}

GraphicsBufferView::GraphicsBufferView(GraphicsBuffer *buffer)
    : m_buffer(buffer)
{
    int width;
    int height;
    int stride;
    int format;

    if (auto dmabuf = buffer->dmabufAttributes()) {
        if (dmabuf->planeCount != 1) {
            return;
        }
        width = dmabuf->width;
        height = dmabuf->height;
        stride = dmabuf->pitch[0];
        format = dmabuf->format;
    } else if (auto shm = buffer->shmAttributes()) {
        width = shm->size.width();
        height = shm->size.height();
        stride = shm->stride;
        format = shm->format;
    } else {
        qCWarning(KWIN_CORE) << "Cannot create a graphics buffer view for unknown buffer type" << buffer;
        return;
    }

    void *data = buffer->map();
    if (data) {
        m_image = QImage(static_cast<uchar *>(data), width, height, stride, drmFormatToQImageFormat(format));
    }
}

GraphicsBufferView::~GraphicsBufferView()
{
    if (!m_image.isNull()) {
        m_buffer->unmap();
    }
}

QImage *GraphicsBufferView::image()
{
    return &m_image;
}

const QImage *GraphicsBufferView::image() const
{
    return &m_image;
}

} // namespace KWin
