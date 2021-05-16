/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platformqpaintersurfacetexture_internal.h"
#include "clientbuffer_internal.h"
#include "surfaceitem_internal.h"

namespace KWin
{

PlatformQPainterSurfaceTextureInternal::PlatformQPainterSurfaceTextureInternal(QPainterBackend *backend,
                                                                               SurfacePixmapInternal *pixmap)
    : PlatformQPainterSurfaceTexture(backend)
    , m_pixmap(pixmap)
{
}

bool PlatformQPainterSurfaceTextureInternal::create()
{
    update(QRegion());
    return !m_image.isNull();
}

void PlatformQPainterSurfaceTextureInternal::update(const QRegion &region)
{
    Q_UNUSED(region)
    const ClientBufferInternal *buffer = ClientBufferInternal::from(m_pixmap->buffer());
    if (buffer) {
        m_image = buffer->image();
    }
}

} // namespace KWin
