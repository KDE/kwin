/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/graphicsbufferview.h"
#include "core/graphicsbuffer.h"
#include "utils/common.h"
#include "utils/drm_format_helper.h"

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
    case DRM_FORMAT_BGR888:
        return QImage::Format_RGB888;
    case DRM_FORMAT_RGB888:
        return QImage::Format_BGR888;
    default:
        return QImage::Format_Invalid;
    }
}

GraphicsBufferView::GraphicsBufferView(GraphicsBuffer *buffer, GraphicsBuffer::MapFlags accessFlags)
    : m_buffer(buffer)
{
    int width;
    int height;
    int format;

    if (auto dmabuf = buffer->dmabufAttributes()) {
        if (dmabuf->planeCount != 1) {
            return;
        }
        width = dmabuf->width;
        height = dmabuf->height;
        format = dmabuf->format;
    } else if (auto shm = buffer->shmAttributes()) {
        width = shm->size.width();
        height = shm->size.height();
        format = shm->format;
    } else {
        qCWarning(KWIN_CORE) << "Cannot create a graphics buffer view for unknown buffer type" << buffer;
        return;
    }

    const auto [data, stride] = buffer->map(accessFlags);
    if (data) {
        m_image = QImage(static_cast<uchar *>(data), width, height, stride, drmFormatToQImageFormat(format));
        if (Q_UNLIKELY(m_image.isNull())) {
            qCWarning(KWIN_CORE) << "Cannot create a graphics buffer view" << buffer << FormatInfo::drmFormatName(format) << drmFormatToQImageFormat(format);
        }
    }
}

GraphicsBufferView::~GraphicsBufferView()
{
    if (!m_image.isNull()) {
        m_buffer->unmap();
    }
}

bool GraphicsBufferView::isNull() const
{
    return m_image.isNull();
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
