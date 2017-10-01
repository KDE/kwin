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
#include "kwinglutils.h"
#include <kwinglutils_export.h>

#include <QSize>
#include <QSharedData>
#include <QImage>
#include <QMatrix4x4>
#include <epoxy/gl.h>

namespace KWin
{
// forward declarations
class GLVertexBuffer;

class KWINGLUTILS_EXPORT GLTexturePrivate
    : public QSharedData
{
public:
    GLTexturePrivate();
    virtual ~GLTexturePrivate();

    virtual void onDamage();

    void updateMatrix();

    GLuint m_texture;
    GLenum m_target;
    GLenum m_internalFormat;
    GLenum m_filter;
    GLenum m_wrapMode;
    QSize m_size;
    QSizeF m_scale; // to un-normalize GL_TEXTURE_2D
    QMatrix4x4 m_matrix[2];
    bool m_yInverted; // texture is y-inverted
    bool m_canUseMipmaps;
    bool m_markedDirty;
    bool m_filterChanged;
    bool m_wrapModeChanged;
    bool m_immutable;
    int m_mipLevels;

    int m_unnormalizeActive; // 0 - no, otherwise refcount
    int m_normalizeActive; // 0 - no, otherwise refcount
    GLVertexBuffer* m_vbo;
    QSize m_cachedSize;

    static void initStatic();

    static bool s_supportsFramebufferObjects;
    static bool s_supportsARGB32;
    static bool s_supportsUnpack;
    static bool s_supportsTextureStorage;
    static bool s_supportsTextureSwizzle;
    static bool s_supportsTextureFormatRG;
    static GLuint s_fbo;
    static uint s_textureObjectCounter;
private:
    friend void KWin::cleanupGL();
    static void cleanup();
    Q_DISABLE_COPY(GLTexturePrivate)
};

} // namespace

#endif
