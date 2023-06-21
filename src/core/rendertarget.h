/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/colorspace.h"
#include "core/output.h"
#include "vulkan/vulkan_include.h"

#include <QImage>

namespace KWin
{

class GLFramebuffer;
class GLTexture;

class KWIN_EXPORT RenderTarget
{
public:
    explicit RenderTarget(GLFramebuffer *fbo, const ColorDescription &colorDescription = ColorDescription::sRGB);
    explicit RenderTarget(QImage *image, const ColorDescription &colorDescription = ColorDescription::sRGB);
    explicit RenderTarget(vk::CommandBuffer cmd, vk::ImageView view, const QSize &framebufferSize, const ColorDescription &colorDescription = ColorDescription::sRGB);

    QSize size() const;
    OutputTransform transform() const;
    const ColorDescription &colorDescription() const;

    QImage *image() const;
    GLFramebuffer *framebuffer() const;
    GLTexture *texture() const;
    vk::CommandBuffer commandBuffer() const;
    vk::ImageView imageView() const;

private:
    const QSize m_size;
    QImage *m_image = nullptr;
    GLFramebuffer *m_framebuffer = nullptr;
    const OutputTransform m_transform;
    vk::CommandBuffer m_commandBuffer;
    vk::ImageView m_imageView;
    const ColorDescription m_colorDescription;
};

} // namespace KWin
