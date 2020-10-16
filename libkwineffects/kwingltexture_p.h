/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2011 Philipp Knechtges <philipp-dev@knechtges.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
    bool m_foreign;
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
