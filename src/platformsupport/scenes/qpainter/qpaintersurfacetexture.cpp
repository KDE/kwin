/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "qpaintersurfacetexture.h"

namespace KWin
{

QPainterSurfaceTexture::QPainterSurfaceTexture(QPainterBackend *backend)
    : m_backend(backend)
{
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
