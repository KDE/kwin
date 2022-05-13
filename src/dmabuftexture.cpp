/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "dmabuftexture.h"

#include "kwinglutils.h"

#include <unistd.h>

namespace KWin
{

DmaBufTexture::DmaBufTexture(QSharedPointer<GLTexture> texture, const DmaBufAttributes &attributes)
    : m_texture(texture)
    , m_framebuffer(new GLFramebuffer(texture.data()))
    , m_attributes(attributes)
{
}

DmaBufTexture::~DmaBufTexture()
{
    for (int i = 0; i < m_attributes.planeCount; ++i) {
        ::close(m_attributes.fd[i]);
    }
}

DmaBufAttributes DmaBufTexture::attributes() const
{
    return m_attributes;
}

KWin::GLTexture *DmaBufTexture::texture() const
{
    return m_texture.data();
}

KWin::GLFramebuffer *DmaBufTexture::framebuffer() const
{
    return m_framebuffer.data();
}

} // namespace KWin
