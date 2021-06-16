/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "qpaintersurfacetextureprovider.h"

namespace KWin
{

QPainterSurfaceTextureProvider::QPainterSurfaceTextureProvider(QPainterBackend *backend)
    : m_backend(backend)
{
}

bool QPainterSurfaceTextureProvider::isValid() const
{
    return !m_image.isNull();
}

QPainterBackend *QPainterSurfaceTextureProvider::backend() const
{
    return m_backend;
}

QImage QPainterSurfaceTextureProvider::image() const
{
    return m_image;
}

} // namespace KWin
