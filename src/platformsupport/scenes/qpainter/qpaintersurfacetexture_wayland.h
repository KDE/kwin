/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "qpaintersurfacetexture.h"

namespace KWin
{

class SurfacePixmapWayland;

class KWIN_EXPORT QPainterSurfaceTextureWayland : public QPainterSurfaceTexture
{
public:
    QPainterSurfaceTextureWayland(QPainterBackend *backend, SurfacePixmapWayland *pixmap);

    bool create() override;
    void update(const QRegion &region) override;

private:
    SurfacePixmapWayland *m_pixmap;
};

} // namespace KWin
