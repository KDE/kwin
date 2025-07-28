/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/colorspace.h"
#include "core/output.h"

#include <QImage>

namespace KWin
{

class GLFramebuffer;
class GLTexture;

class KWIN_EXPORT RenderTarget
{
public:
    explicit RenderTarget(GLFramebuffer *fbo, const std::shared_ptr<ColorDescription> &colorDescription = ColorDescription::sRGB);
    explicit RenderTarget(QImage *image, const std::shared_ptr<ColorDescription> &colorDescription = ColorDescription::sRGB);

    QSize transformedSize() const;
    QRect transformedRect() const;

    QSize size() const;
    OutputTransform transform() const;
    const std::shared_ptr<ColorDescription> &colorDescription() const;

    QImage *image() const;
    GLFramebuffer *framebuffer() const;
    GLTexture *texture() const;

private:
    QImage *m_image = nullptr;
    GLFramebuffer *m_framebuffer = nullptr;
    const OutputTransform m_transform;
    const std::shared_ptr<ColorDescription> m_colorDescription;
};

} // namespace KWin
