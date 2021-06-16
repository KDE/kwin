/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "qpaintersurfacetextureprovider.h"

namespace KWin
{

class KWIN_EXPORT QPainterSurfaceTextureProviderInternal : public QPainterSurfaceTextureProvider
{
public:
    QPainterSurfaceTextureProviderInternal(QPainterBackend *backend, SurfacePixmapInternal *pixmap);

    bool create() override;
    void update(const QRegion &region) override;

private:
    SurfacePixmapInternal *m_pixmap;
};

} // namespace KWin
