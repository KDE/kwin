/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/surfaceitem.h"

#include <QImage>

namespace KWin
{

class QPainterBackend;

class KWIN_EXPORT QPainterSurfaceTexture : public SurfaceTexture
{
public:
    QPainterSurfaceTexture(QPainterBackend *backend, SurfacePixmap *pixmap);

    bool create() override;
    void update(const QRegion &region) override;
    bool isValid() const override;

    QPainterBackend *backend() const;
    QImage image() const;

protected:
    QPainterBackend *m_backend;
    SurfacePixmap *m_pixmap;
    QImage m_image;
};

} // namespace KWin
