/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/output.h"

#include <QExplicitlySharedDataPointer>
#include <QMatrix4x4>
#include <QRegion>
#include <QSize>

#include <epoxy/gl.h>

class QImage;
class QPixmap;

/** @addtogroup kwineffects */
/** @{ */

namespace KWin
{

class GLVertexBuffer;
class GLTexturePrivate;

enum TextureCoordinateType {
    NormalizedCoordinates = 0,
    UnnormalizedCoordinates,
};

class KWIN_EXPORT GLTexture
{
public:
    explicit GLTexture(GLenum target);

    /**
     * Creates the underlying texture object. Returns @c true if the texture has been created
     * successfully; otherwise returns @c false. Note that this does not allocate any storage
     * for the texture.
     */
    bool create();
    virtual ~GLTexture();

    bool isNull() const;
    QSize size() const;
    void setSize(const QSize &size);
    int width() const;
    int height() const;

    /**
     * sets the transform between the content and the buffer
     */
    void setContentTransform(OutputTransform transform);

    /**
     * @returns the transform between the content and the buffer
     */
    OutputTransform contentTransform() const;

    /**
     * Specifies which component of a texel is placed in each respective
     * component of the vector returned to the shader.
     *
     * Valid values are GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA, GL_ONE and GL_ZERO.
     *
     * @see swizzleSupported()
     * @since 5.2
     */
    void setSwizzle(GLenum red, GLenum green, GLenum blue, GLenum alpha);

    /**
     * Returns a matrix that transforms texture coordinates of the given type,
     * taking the texture target and the y-inversion flag into account.
     *
     * @since 4.11
     */
    QMatrix4x4 matrix(TextureCoordinateType type) const;

    void update(const QImage &image, const QRegion &region, const QPoint &offset = QPoint());
    void bind();
    void unbind();
    void render(const QSizeF &size);
    void render(const QRegion &region, const QSizeF &size, bool hardwareClipping = false);
    void render(const QRectF &source, const QRegion &region, const QSizeF &targetSize, bool hardwareClipping = false);

    GLuint texture() const;
    GLenum target() const;
    GLenum filter() const;
    GLenum internalFormat() const;

    QImage toImage();

    /**
     * @deprecated track modifications to the texture yourself
     */
    void setDirty();
    bool isDirty() const;
    void setFilter(GLenum filter);
    void setWrapMode(GLenum mode);

    void generateMipmaps();

    /**
     * Returns true if texture swizzle is supported, and false otherwise
     *
     * Texture swizzle requires OpenGL 3.3, GL_ARB_texture_swizzle, or OpenGL ES 3.0.
     *
     * @since 5.2
     */
    static bool supportsSwizzle();

    /**
     * Returns @c true if texture formats R* are supported, and @c false otherwise.
     *
     * This requires OpenGL 3.0, GL_ARB_texture_rg or OpenGL ES 3.0 or GL_EXT_texture_rg.
     *
     * @since 5.2.1
     */
    static bool supportsFormatRG();

    static std::unique_ptr<GLTexture> createNonOwningWrapper(GLuint textureId, GLenum internalFormat, const QSize &size);
    static std::unique_ptr<GLTexture> allocate(GLenum internalFormat, const QSize &size, int levels = 1);
    static std::unique_ptr<GLTexture> upload(const QImage &image);
    static std::unique_ptr<GLTexture> upload(const QPixmap &pixmap);

protected:
    explicit GLTexture(GLenum target, GLuint textureId, GLenum internalFormat, const QSize &size, int levels, bool owning, OutputTransform transform);

    const std::unique_ptr<GLTexturePrivate> d;

    virtual void onDamage();
};

} // namespace

/** @} */
