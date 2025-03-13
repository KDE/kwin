/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/graphicsbufferview.h"
#include "scene/surfaceitem.h"

namespace KWin
{

class QPainterBackend;

class KWIN_EXPORT QPainterSurfaceTexture : public SurfaceTexture
{
public:
    explicit QPainterSurfaceTexture(QPainterBackend *backend, SurfacePixmap *pixmap);

    bool isValid() const override;
    bool create() override;
    void update(const QRegion &region) override;

    QPainterBackend *backend() const;
    const GraphicsBufferView &view() const;

protected:
    QPainterBackend *m_backend;
    SurfacePixmap *m_pixmap;
    GraphicsBufferView m_view;
};

} // namespace KWin
