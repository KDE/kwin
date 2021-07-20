/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platformqpaintersurfacetexture_wayland.h"
#include "surfaceitem_wayland.h"

#include <KWaylandServer/shmclientbuffer.h>
#include <KWaylandServer/surface_interface.h>

#include <QPainter>

namespace KWin
{

PlatformQPainterSurfaceTextureWayland::PlatformQPainterSurfaceTextureWayland(QPainterBackend *backend,
                                                                             SurfacePixmapWayland *pixmap)
    : PlatformQPainterSurfaceTexture(backend)
    , m_pixmap(pixmap)
{
}

bool PlatformQPainterSurfaceTextureWayland::create()
{
    auto buffer = qobject_cast<KWaylandServer::ShmClientBuffer *>(m_pixmap->buffer());
    if (Q_LIKELY(buffer)) {
        // The buffer data is copied as the buffer interface returns a QImage
        // which doesn't own the data of the underlying wl_shm_buffer object.
        m_image = buffer->data().copy();
    }
    return !m_image.isNull();
}

void PlatformQPainterSurfaceTextureWayland::update(const QRegion &region)
{
    auto buffer = qobject_cast<KWaylandServer::ShmClientBuffer *>(m_pixmap->buffer());
    if (Q_UNLIKELY(!buffer)) {
        return;
    }

    const QImage image = buffer->data();
    const QRegion dirtyRegion = mapRegion(m_pixmap->item()->surfaceToBufferMatrix(), region);
    QPainter painter(&m_image);

    // The buffer data is copied as the buffer interface returns a QImage
    // which doesn't own the data of the underlying wl_shm_buffer object.
    for (const QRect &rect : dirtyRegion) {
        painter.drawImage(rect, image, rect);
    }
}

} // namespace KWin
