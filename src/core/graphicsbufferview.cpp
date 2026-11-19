/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/graphicsbufferview.h"
#include "core/drm_formats.h"
#include "core/graphicsbuffer.h"
#include "utils/common.h"

#include <drm_fourcc.h>

namespace KWin
{

GraphicsBufferView::GraphicsBufferView(GraphicsBuffer *buffer, GraphicsBuffer::MapFlags accessFlags)
    : m_buffer(buffer)
{
    const QSize size = buffer->size();
    const uint32_t format = buffer->format();
    const auto info = FormatInfo::get(format);
    if (!info || !info->qimageFormat) {
        return;
    }

    const auto [data, stride] = buffer->map(accessFlags);
    if (data) {
        m_image = QImage(static_cast<uchar *>(data), size.width(), size.height(), stride, *info->qimageFormat);
        if (Q_UNLIKELY(m_image.isNull())) {
            qCWarning(KWIN_CORE) << "Cannot create a graphics buffer view" << buffer << FormatInfo::drmFormatName(format) << *info->qimageFormat;
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
