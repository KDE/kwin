/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/colorspace.h"

#include <QImage>
#include <QMatrix4x4>
#include <variant>

namespace KWin
{

class GLFramebuffer;
class GLTexture;

class KWIN_EXPORT RenderTarget
{
public:
    explicit RenderTarget(GLFramebuffer *fbo, const ColorDescription &colorDescription = ColorDescription::sRGBf());
    explicit RenderTarget(QImage *image, const ColorDescription &colorDescription = ColorDescription::sRGBf());

    QSize size() const;
    QMatrix4x4 transformation() const;
    const ColorDescription &colorDescription() const;
    QRectF applyTransformation(const QRectF &rect, const QRectF &viewport) const;
    QRect applyTransformation(const QRect &rect, const QRect &viewport) const;
    QPointF applyTransformation(const QPointF &point, const QRectF &viewport) const;
    QPoint applyTransformation(const QPoint &point, const QRect &viewport) const;

    QImage *image() const;
    GLFramebuffer *framebuffer() const;
    GLTexture *texture() const;

private:
    QImage *m_image = nullptr;
    GLFramebuffer *m_framebuffer = nullptr;
    QMatrix4x4 m_transformation;
    const ColorDescription m_colorDescription;
};

} // namespace KWin
