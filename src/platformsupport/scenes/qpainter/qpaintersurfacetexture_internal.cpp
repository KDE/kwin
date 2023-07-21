/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "qpaintersurfacetexture_internal.h"
#include "core/graphicsbufferview.h"
#include "scene/surfaceitem_internal.h"

namespace KWin
{

QPainterSurfaceTextureInternal::QPainterSurfaceTextureInternal(QPainterBackend *backend,
                                                               SurfacePixmapInternal *pixmap)
    : QPainterSurfaceTexture(backend)
    , m_pixmap(pixmap)
{
}

bool QPainterSurfaceTextureInternal::create()
{
    update(QRegion());
    return !m_image.isNull();
}

void QPainterSurfaceTextureInternal::update(const QRegion &region)
{
    if (!m_pixmap->graphicsBuffer()) {
        return;
    }

    const GraphicsBufferView view(m_pixmap->graphicsBuffer());
    if (view.isNull()) {
        return;
    }

    m_image = view.image()->copy();
}

} // namespace KWin
