/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "dmabuftexture.h"

#include "kwinglutils.h"

#include <unistd.h>

namespace KWin
{

DmaBufTexture::DmaBufTexture(std::shared_ptr<GLTexture> texture, DmaBufAttributes &&attributes)
    : m_texture(texture)
    , m_framebuffer(std::make_unique<GLFramebuffer>(texture.get()))
    , m_attributes(std::move(attributes))
{
}
DmaBufTexture::~DmaBufTexture() = default;

const DmaBufAttributes &DmaBufTexture::attributes() const
{
    return m_attributes;
}

KWin::GLTexture *DmaBufTexture::texture() const
{
    return m_texture.get();
}

KWin::GLFramebuffer *DmaBufTexture::framebuffer() const
{
    return m_framebuffer.get();
}

} // namespace KWin
