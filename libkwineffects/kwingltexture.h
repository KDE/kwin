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

#include <QtCore/QSize>
#include <QSharedPointer>
#include <QExplicitlySharedDataPointer>

class QImage;
class QPixmap;

/** @addtogroup kwineffects */
/** @{ */

namespace KWin
{

class GLVertexBuffer;
class GLTexturePrivate;

class KWIN_EXPORT GLTexture
{
public:
    GLTexture();
    GLTexture(const GLTexture& tex);
    explicit GLTexture(const QImage& image, GLenum target = GL_TEXTURE_2D);
    explicit GLTexture(const QPixmap& pixmap, GLenum target = GL_TEXTURE_2D);
    GLTexture(const QString& fileName);
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

    virtual bool load(const QImage& image, GLenum target = GL_TEXTURE_2D);
    virtual bool load(const QPixmap& pixmap, GLenum target = GL_TEXTURE_2D);
    virtual bool load(const QString& fileName);
    virtual void discard();
    void bind();
    void unbind();
    /**
     * Release the data which is bound to the texture.
     * @since 4.8
     **/
    void release();
    void render(QRegion region, const QRect& rect);

    GLuint texture() const;
    GLenum target() const;
    GLenum filter() const;
    virtual bool isDirty() const;
    void setTexture(GLuint texture);
    void setTarget(GLenum target);
    void setFilter(GLenum filter);
    void setWrapMode(GLenum mode);
    virtual void setDirty();

    static bool NPOTTextureSupported();
    static bool framebufferObjectSupported();
    static bool saturationSupported();

protected:
    void enableFilter();
    QImage convertToGLFormat(const QImage& img) const;

    QExplicitlySharedDataPointer<GLTexturePrivate> d_ptr;
    GLTexture(GLTexturePrivate& dd);

private:
    Q_DECLARE_PRIVATE(GLTexture)
};

} // namespace

/** @} */

#endif
