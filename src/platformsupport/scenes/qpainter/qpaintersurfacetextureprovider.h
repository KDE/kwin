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

class KWIN_EXPORT QPainterSurfaceTextureProvider : public SurfaceTextureProvider
{
    Q_OBJECT

public:
    explicit QPainterSurfaceTextureProvider(QPainterBackend *backend);
    ~QPainterSurfaceTextureProvider() override;

    bool isValid() const override;
    KrkTexture *texture() const override;

    QPainterBackend *backend() const;
    QImage image() const;

protected:
    QPainterBackend *m_backend;
    QImage m_image;
    QScopedPointer<KrkTexture> m_sceneTexture;
};

} // namespace KWin
