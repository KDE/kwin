/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "qpaintersurfacetexture.h"

namespace KWin
{

QPainterSurfaceTexture::QPainterSurfaceTexture(QPainterBackend *backend, SurfacePixmap *pixmap)
    : m_backend(backend)
    , m_pixmap(pixmap)
{
}

bool QPainterSurfaceTexture::isValid() const
{
    return !m_view.isNull();
}

bool QPainterSurfaceTexture::create()
{
    m_view = GraphicsBufferView(m_pixmap->item()->buffer());
    return !m_view.isNull();
}

void QPainterSurfaceTexture::update(const QRegion &region)
{
    m_view = GraphicsBufferView(m_pixmap->item()->buffer());
}

QPainterBackend *QPainterSurfaceTexture::backend() const
{
    return m_backend;
}

const GraphicsBufferView &QPainterSurfaceTexture::view() const
{
    return m_view;
}

} // namespace KWin
