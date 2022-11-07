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

#include <QImage>
#include <QMatrix4x4>
#include <QSharedData>
#include <QSize>
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
    std::unique_ptr<GLVertexBuffer> m_vbo;
    QSize m_cachedSize;

    static void initStatic();

    static bool s_supportsFramebufferObjects;
    static bool s_supportsARGB32;
    static bool s_supportsUnpack;
    static bool s_supportsTextureStorage;
    static bool s_supportsTextureSwizzle;
    static bool s_supportsTextureFormatRG;
    static bool s_supportsTexture16Bit;
    static GLuint s_fbo;

    // Table of GL formats/types associated with different values of QImage::Format.
    // Zero values indicate a direct upload is not feasible.
    //
    // Note: Blending is set up to expect premultiplied data, so the non-premultiplied
    // Format_ARGB32 must be converted to Format_ARGB32_Premultiplied ahead of time.
    struct
    {
        GLenum internalFormat;
        GLenum format;
        GLenum type;
    } static constexpr s_formatTable[] = {
        {0, 0, 0}, // QImage::Format_Invalid
        {0, 0, 0}, // QImage::Format_Mono
        {0, 0, 0}, // QImage::Format_MonoLSB
        {0, 0, 0}, // QImage::Format_Indexed8
        {GL_RGB8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV}, // QImage::Format_RGB32
        {0, 0, 0}, // QImage::Format_ARGB32
        {GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV}, // QImage::Format_ARGB32_Premultiplied
        {GL_RGB8, GL_BGR, GL_UNSIGNED_SHORT_5_6_5_REV}, // QImage::Format_RGB16
        {0, 0, 0}, // QImage::Format_ARGB8565_Premultiplied
        {0, 0, 0}, // QImage::Format_RGB666
        {0, 0, 0}, // QImage::Format_ARGB6666_Premultiplied
        {GL_RGB5, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV}, // QImage::Format_RGB555
        {0, 0, 0}, // QImage::Format_ARGB8555_Premultiplied
        {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE}, // QImage::Format_RGB888
        {GL_RGB4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV}, // QImage::Format_RGB444
        {GL_RGBA4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV}, // QImage::Format_ARGB4444_Premultiplied
        {GL_RGB8, GL_RGBA, GL_UNSIGNED_BYTE}, // QImage::Format_RGBX8888
        {0, 0, 0}, // QImage::Format_RGBA8888
        {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}, // QImage::Format_RGBA8888_Premultiplied
        {GL_RGB10, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV}, // QImage::Format_BGR30
        {GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV}, // QImage::Format_A2BGR30_Premultiplied
        {GL_RGB10, GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV}, // QImage::Format_RGB30
        {GL_RGB10_A2, GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV}, // QImage::Format_A2RGB30_Premultiplied
        {GL_R8, GL_RED, GL_UNSIGNED_BYTE}, // QImage::Format_Alpha8
        {GL_R8, GL_RED, GL_UNSIGNED_BYTE}, // QImage::Format_Grayscale8
        {GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT}, // QImage::Format_RGBX64
        {0, 0, 0}, // QImage::Format_RGBA64
        {GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT}, // QImage::Format_RGBA64_Premultiplied
        {GL_R16, GL_RED, GL_UNSIGNED_SHORT}, // QImage::Format_Grayscale16
        {0, 0, 0}, // QImage::Format_BGR888
    };

private:
    friend void KWin::cleanupGL();
    static void cleanup();
    Q_DISABLE_COPY(GLTexturePrivate)
};

} // namespace

#endif
