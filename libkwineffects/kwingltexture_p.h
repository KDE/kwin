/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006-2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2011 Philipp Knechtges <philipp-dev@knechtges.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef KWIN_GLTEXTURE_P_H
#define KWIN_GLTEXTURE_P_H

#include "kwinconfig.h" // KWIN_HAVE_OPENGL
#include "kwinglobals.h"

#include <QtCore/QSize>
#include <QtCore/QSharedData>

namespace KWin
{

class KWIN_EXPORT GLTexturePrivate
    : public QSharedData
{
public:
    GLTexturePrivate();
    virtual ~GLTexturePrivate();

    virtual void bind();
    virtual void unbind();
    virtual void release();

    void enableFilter();
    QImage convertToGLFormat(const QImage& img) const;

    GLuint m_texture;
    GLenum m_target;
    GLenum m_filter;
    QSize m_size;
    QSizeF m_scale; // to un-normalize GL_TEXTURE_2D
    bool m_yInverted; // texture has y inverted
    bool m_canUseMipmaps;
    bool m_hasValidMipmaps;

    int m_unnormalizeActive; // 0 - no, otherwise refcount
    int m_normalizeActive; // 0 - no, otherwise refcount
    GLVertexBuffer* m_vbo;
    QSize m_cachedSize;

    static void initStatic();

    static bool sNPOTTextureSupported;
    static bool sFramebufferObjectSupported;
    static bool sSaturationSupported;
private:
    Q_DISABLE_COPY(GLTexturePrivate)
};

} // namespace

#endif
