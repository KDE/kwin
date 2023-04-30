/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "libkwineffects/colorspace.h"
#include "libkwineffects/kwinglutils_export.h"

#include <QImage>
#include <QMatrix4x4>
#include <variant>

namespace KWin
{

class GLFramebuffer;
class GLTexture;

class KWINGLUTILS_EXPORT RenderTarget
{
public:
    explicit RenderTarget(GLFramebuffer *fbo, const Colorspace &colorspace = Colorspace::sRGB, uint32_t sdrBrightness = 200);
    explicit RenderTarget(QImage *image, const Colorspace &colorspace = Colorspace::sRGB, uint32_t sdrBrightness = 200);

    QSize size() const;
    QMatrix4x4 transformation() const;
    const Colorspace &colorspace() const;
    uint32_t sdrBrightness() const;
    QRectF applyTransformation(const QRectF &rect, const QRectF &viewport) const;
    QRect applyTransformation(const QRect &rect, const QRect &viewport) const;

    QImage *image() const;
    GLFramebuffer *framebuffer() const;
    GLTexture *texture() const;

private:
    QImage *m_image = nullptr;
    GLFramebuffer *m_framebuffer = nullptr;
    QMatrix4x4 m_transformation;
    const Colorspace m_colorspace;
    const uint32_t m_sdrBrightness;
};

} // namespace KWin
