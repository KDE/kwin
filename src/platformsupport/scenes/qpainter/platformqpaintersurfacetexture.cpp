/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platformqpaintersurfacetexture.h"

namespace KWin
{

PlatformQPainterSurfaceTexture::PlatformQPainterSurfaceTexture(QPainterBackend *backend)
    : m_backend(backend)
{
}

bool PlatformQPainterSurfaceTexture::isValid() const
{
    return !m_image.isNull();
}

QPainterBackend *PlatformQPainterSurfaceTexture::backend() const
{
    return m_backend;
}

QImage PlatformQPainterSurfaceTexture::image() const
{
    return m_image;
}

} // namespace KWin
