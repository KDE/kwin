/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/qpainter/texture.h"
#include "core/graphicsbufferview.h"
#include "core/region.h"

#include <QPainter>

namespace KWin
{

QImage TextureQPainter::nativeImage() const
{
    return m_image;
}

std::unique_ptr<ImageTextureQPainter> ImageTextureQPainter::create(const QImage &image)
{
    auto texture = std::make_unique<ImageTextureQPainter>();
    texture->upload(image);
    return texture;
}

void ImageTextureQPainter::attach(GraphicsBuffer *buffer, const Region &region)
{
    Q_UNREACHABLE();
}

void ImageTextureQPainter::upload(const QImage &image)
{
    m_image = image;
}

void ImageTextureQPainter::upload(const QImage &image, const Rect &rect)
{
    m_image = image;
}

std::unique_ptr<BufferTextureQPainter> BufferTextureQPainter::create(GraphicsBuffer *buffer)
{
    auto texture = std::make_unique<BufferTextureQPainter>();
    if (texture->attach(buffer)) {
        return texture;
    }

    return nullptr;
}

bool BufferTextureQPainter::attach(GraphicsBuffer *buffer)
{
    const GraphicsBufferView view(buffer);
    if (Q_LIKELY(!view.isNull())) {
        // The buffer data is copied as the buffer interface returns a QImage
        // which doesn't own the data of the underlying wl_shm_buffer object.
        m_image = view.image()->copy();
    }
    m_size = m_image.size();
    return !m_image.isNull();
}

void BufferTextureQPainter::attach(GraphicsBuffer *buffer, const Region &region)
{
    const GraphicsBufferView view(buffer);
    if (Q_UNLIKELY(view.isNull())) {
        return;
    }

    QPainter painter(&m_image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    // The buffer data is copied as the buffer interface returns a QImage
    // which doesn't own the data of the underlying wl_shm_buffer object.
    for (const Rect &rect : region.rects()) {
        painter.drawImage(rect, *view.image(), rect);
    }
}

void BufferTextureQPainter::upload(const QImage &image, const Rect &rect)
{
    Q_UNREACHABLE();
}

} // namespace KWin
