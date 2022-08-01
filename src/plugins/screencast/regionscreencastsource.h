/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "screencastsource.h"

#include <QImage>
#include <kwingltexture.h>
#include <kwinglutils.h>

namespace KWin
{
class Output;

class RegionScreenCastSource : public ScreenCastSource
{
    Q_OBJECT

public:
    explicit RegionScreenCastSource(const QRect &region, qreal scale, QObject *parent = nullptr);

    bool hasAlphaChannel() const override;
    QSize textureSize() const override;

    void render(GLFramebuffer *target) override;
    void render(QImage *image) override;
    std::chrono::nanoseconds clock() const override;

    QRect region() const
    {
        return m_region;
    }
    void updateOutput(Output *output);

private:
    const QRect m_region;
    const qreal m_scale;
    std::unique_ptr<GLFramebuffer> m_target;
    std::unique_ptr<GLTexture> m_renderedTexture;
    std::chrono::nanoseconds m_last;
};

} // namespace KWin
