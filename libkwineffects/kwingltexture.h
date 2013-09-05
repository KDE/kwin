/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006-2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef KWIN_GLTEXTURE_H
#define KWIN_GLTEXTURE_H

#include "kwinglobals.h"

#include <QSize>
#include <QRegion>
#include <QSharedPointer>
#include <QExplicitlySharedDataPointer>
#include <QtGui/QMatrix4x4>

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
    UnnormalizedCoordinates
};

class KWIN_EXPORT GLTexture
{
public:
    GLTexture();
    GLTexture(const GLTexture& tex);
    explicit GLTexture(const QImage& image, GLenum target = GL_TEXTURE_2D);
    explicit GLTexture(const QPixmap& pixmap, GLenum target = GL_TEXTURE_2D);
    explicit GLTexture(const QString& fileName);
    GLTexture(int width, int height);
    virtual ~GLTexture();

    GLTexture & operator = (const GLTexture& tex);

    bool isNull() const;
    QSize size() const;
    int width() const;
    int height() const;
    /**
     * @since 4.7
     **/
    bool isYInverted() const;
    /**
     * @since 4.8
     **/
    void setYInverted(bool inverted);

    /**
     * Returns a matrix that transforms texture coordinates of the given type,
     * taking the texture target and the y-inversion flag into account.
     *
     * @since 4.11
     */
    QMatrix4x4 matrix(TextureCoordinateType type) const;

    virtual bool load(const QImage& image, GLenum target = GL_TEXTURE_2D);
    virtual bool load(const QPixmap& pixmap, GLenum target = GL_TEXTURE_2D);
    virtual bool load(const QString& fileName);
    void update(const QImage& image, const QPoint &offset = QPoint(0, 0), const QRect &src = QRect());
    virtual void discard();
    void bind();
    void unbind();
    void render(QRegion region, const QRect& rect, bool hardwareClipping = false);

    GLuint texture() const;
    GLenum target() const;
    GLenum filter() const;
    /** @short
     * Make the texture fully transparent
     * Warning: this clobbers the current framebuffer binding except on fglrx
     */
    void clear();
    bool isDirty() const;
    void setFilter(GLenum filter);
    void setWrapMode(GLenum mode);
    void setDirty();

    static bool NPOTTextureSupported();
    static bool framebufferObjectSupported();
    static bool saturationSupported();

protected:
    QExplicitlySharedDataPointer<GLTexturePrivate> d_ptr;
    GLTexture(GLTexturePrivate& dd);

private:
    Q_DECLARE_PRIVATE(GLTexture)
};

} // namespace

/** @} */

#endif
