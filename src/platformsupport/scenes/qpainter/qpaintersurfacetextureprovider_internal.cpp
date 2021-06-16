/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "qpaintersurfacetextureprovider_internal.h"
#include "surfaceitem_internal.h"

namespace KWin
{

QPainterSurfaceTextureProviderInternal::QPainterSurfaceTextureProviderInternal(QPainterBackend *backend,
                                                                               SurfacePixmapInternal *pixmap)
    : QPainterSurfaceTextureProvider(backend)
    , m_pixmap(pixmap)
{
}

bool QPainterSurfaceTextureProviderInternal::create()
{
    update(QRegion());
    return !m_image.isNull();
}

void QPainterSurfaceTextureProviderInternal::update(const QRegion &region)
{
    Q_UNUSED(region)
    m_image = m_pixmap->image();
}

} // namespace KWin
