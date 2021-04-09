/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "surfaceitem.h"

#include <QImage>

namespace KWin
{

class QPainterBackend;

class KWIN_EXPORT PlatformQPainterSurfaceTexture : public PlatformSurfaceTexture
{
public:
    explicit PlatformQPainterSurfaceTexture(QPainterBackend *backend);

    bool isValid() const override;

    QPainterBackend *backend() const;
    QImage image() const;

    virtual bool create() = 0;
    virtual void update(const QRegion &region) = 0;

protected:
    QPainterBackend *m_backend;
    QImage m_image;
};

} // namespace KWin
