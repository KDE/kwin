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

#include "kwinconfig.h" // KWIN_HAVE_OPENGL
#include "kwinglobals.h"

#ifdef KWIN_HAVE_OPENGL

#include <QtCore/QSize>
#include <QtCore/QSharedData>

class QImage;
class QPixmap;

/** @addtogroup kwineffects */
/** @{ */

namespace KWin
{

class GLVertexBuffer;

class KWIN_EXPORT GLTexture
    : public QSharedData
{
public:
    GLTexture();
    explicit GLTexture(const QImage& image, GLenum target = GL_TEXTURE_2D);
    explicit GLTexture(const QPixmap& pixmap, GLenum target = GL_TEXTURE_2D);
    GLTexture(const QString& fileName);
    GLTexture(int width, int height);
    virtual ~GLTexture();

    bool isNull() const;
    QSize size() const;
    int width() const {
        return mSize.width();    /// @since 4.5
    }
    int height() const {
        return mSize.height();    /// @since 4.5
    }
    /**
     * @since 4.7
     **/
    bool isYInverted() const;

    virtual bool load(const QImage& image, GLenum target = GL_TEXTURE_2D);
    virtual bool load(const QPixmap& pixmap, GLenum target = GL_TEXTURE_2D);
    virtual bool load(const QString& fileName);
    virtual void discard();
    virtual void bind();
    virtual void unbind();
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

    static void initStatic();
    static bool NPOTTextureSupported()  {
        return sNPOTTextureSupported;
    }
    static bool framebufferObjectSupported()  {
        return sFramebufferObjectSupported;
    }
    static bool saturationSupported()  {
        return sSaturationSupported;
    }

protected:
    void enableFilter();
    QImage convertToGLFormat(const QImage& img) const;

    GLuint mTexture;
    GLenum mTarget;
    GLenum mFilter;
    QSize mSize;
    QSizeF mScale; // to un-normalize GL_TEXTURE_2D
    bool y_inverted; // texture has y inverted
    bool can_use_mipmaps;
    bool has_valid_mipmaps;

private:
    void init();
    int mUnnormalizeActive; // 0 - no, otherwise refcount
    int mNormalizeActive; // 0 - no, otherwise refcount
    GLVertexBuffer* m_vbo;
    QSize m_cachedSize;

    static bool sNPOTTextureSupported;
    static bool sFramebufferObjectSupported;
    static bool sSaturationSupported;
    Q_DISABLE_COPY(GLTexture)
};

} // namespace


#endif

/** @} */

#endif
