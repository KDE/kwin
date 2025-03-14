/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "qpaintersurfacetexture.h"
#include "core/graphicsbufferview.h"

#include <QPainter>

namespace KWin
{

QPainterSurfaceTexture::QPainterSurfaceTexture(QPainterBackend *backend, SurfacePixmap *pixmap)
    : m_backend(backend)
    , m_pixmap(pixmap)
{
}

bool QPainterSurfaceTexture::create()
{
    const GraphicsBufferView view(m_pixmap->item()->buffer());
    if (Q_LIKELY(!view.isNull())) {
        // The buffer data is copied as the buffer interface returns a QImage
        // which doesn't own the data of the underlying wl_shm_buffer object.
        m_image = view.image()->copy();
    }
    return !m_image.isNull();
}

void QPainterSurfaceTexture::update(const QRegion &region)
{
    const GraphicsBufferView view(m_pixmap->item()->buffer());
    if (Q_UNLIKELY(view.isNull())) {
        return;
    }

    QPainter painter(&m_image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    // The buffer data is copied as the buffer interface returns a QImage
    // which doesn't own the data of the underlying wl_shm_buffer object.
    for (const QRect &rect : region) {
        painter.drawImage(rect, *view.image(), rect);
    }
}

bool QPainterSurfaceTexture::isValid() const
{
    return !m_image.isNull();
}

QPainterBackend *QPainterSurfaceTexture::backend() const
{
    return m_backend;
}

QImage QPainterSurfaceTexture::image() const
{
    return m_image;
}

} // namespace KWin
