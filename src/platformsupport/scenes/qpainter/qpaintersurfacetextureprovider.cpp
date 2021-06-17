/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "qpaintersurfacetextureprovider.h"
#include "krktexture.h"

namespace KWin
{

QPainterSurfaceTextureProvider::QPainterSurfaceTextureProvider(QPainterBackend *backend)
    : m_backend(backend)
{
}

QPainterSurfaceTextureProvider::~QPainterSurfaceTextureProvider()
{
}

bool QPainterSurfaceTextureProvider::isValid() const
{
    return !m_image.isNull();
}

KrkTexture *QPainterSurfaceTextureProvider::texture() const
{
    return m_sceneTexture.data();
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
