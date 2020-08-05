/*
 * Copyright © 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Aleix Pol Gonzalez <aleixpol@kde.org>
 */

#include "dmabuftexture.h"

#include "kwineglimagetexture.h"
#include "kwinglutils.h"

using namespace KWin;

DmaBufTexture::DmaBufTexture(KWin::GLTexture *texture)
    : m_texture(texture)
    , m_framebuffer(new KWin::GLRenderTarget(*m_texture))
{
}

DmaBufTexture::~DmaBufTexture() = default;

KWin::GLRenderTarget *DmaBufTexture::framebuffer() const
{
    return m_framebuffer.data();
}

