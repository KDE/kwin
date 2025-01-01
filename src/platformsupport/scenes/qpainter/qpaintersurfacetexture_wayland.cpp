/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "qpaintersurfacetexture_wayland.h"
#include "core/graphicsbufferview.h"

#include <QPainter>

namespace KWin
{

QPainterSurfaceTextureWayland::QPainterSurfaceTextureWayland(QPainterBackend *backend, SurfacePixmap *pixmap)
    : QPainterSurfaceTexture(backend)
    , m_pixmap(pixmap)
{
}

bool QPainterSurfaceTextureWayland::create()
{
    const GraphicsBufferView view(m_pixmap->item()->buffer());
    if (Q_LIKELY(!view.isNull())) {
        // The buffer data is copied as the buffer interface returns a QImage
        // which doesn't own the data of the underlying wl_shm_buffer object.
        m_image = view.image()->copy();
    }
    return !m_image.isNull();
}

void QPainterSurfaceTextureWayland::update(const QRegion &region)
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

} // namespace KWin
