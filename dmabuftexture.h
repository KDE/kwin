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

#pragma once
#include "kwin_export.h"
#include <QScopedPointer>

namespace KWin
{
class GLRenderTarget;
class GLTexture;

class KWIN_EXPORT DmaBufTexture
{
public:
    explicit DmaBufTexture(KWin::GLTexture* texture);
    virtual ~DmaBufTexture();

    virtual quint32 stride() const = 0;
    virtual int fd() const = 0;
    KWin::GLRenderTarget* framebuffer() const;

protected:
    QScopedPointer<KWin::GLTexture> m_texture;
    QScopedPointer<KWin::GLRenderTarget> m_framebuffer;
};

}
