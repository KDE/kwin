/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "qpaintersurfacetexture_wayland.h"
#include "scene/surfaceitem_wayland.h"
#include "utils/common.h"
#include "wayland/shmclientbuffer.h"
#include "wayland/surface_interface.h"

#include <QPainter>

namespace KWin
{

QPainterSurfaceTextureWayland::QPainterSurfaceTextureWayland(QPainterBackend *backend,
                                                             SurfacePixmapWayland *pixmap)
    : QPainterSurfaceTexture(backend)
    , m_pixmap(pixmap)
{
}

bool QPainterSurfaceTextureWayland::create()
{
    auto buffer = qobject_cast<KWaylandServer::ShmClientBuffer *>(m_pixmap->buffer());
    if (Q_LIKELY(buffer)) {
        // The buffer data is copied as the buffer interface returns a QImage
        // which doesn't own the data of the underlying wl_shm_buffer object.
        m_image = buffer->data().copy();
    }
    return !m_image.isNull();
}

void QPainterSurfaceTextureWayland::update(const QRegion &region)
{
    auto buffer = qobject_cast<KWaylandServer::ShmClientBuffer *>(m_pixmap->buffer());
    if (Q_UNLIKELY(!buffer)) {
        return;
    }

    const QImage image = buffer->data();
    const QRegion dirtyRegion = mapRegion(m_pixmap->item()->surfaceToBufferMatrix(), region);
    QPainter painter(&m_image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    // The buffer data is copied as the buffer interface returns a QImage
    // which doesn't own the data of the underlying wl_shm_buffer object.
    for (const QRect &rect : dirtyRegion) {
        painter.drawImage(rect, image, rect);
    }
}

} // namespace KWin
