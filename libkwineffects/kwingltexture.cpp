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

#include "kwinconfig.h" // KWIN_HAVE_OPENGL

#ifdef KWIN_HAVE_OPENGL

#include "kwinglplatform.h"
#include "kwineffects.h"
#include "kwinglutils_funcs.h"
#include "kwinglutils.h"

#include <QPixmap>
#include <QImage>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>

namespace KWin
{

//****************************************
// GLTexture
//****************************************

bool GLTexture::sNPOTTextureSupported = false;
bool GLTexture::sFramebufferObjectSupported = false;
bool GLTexture::sSaturationSupported = false;

GLTexture::GLTexture()
{
    init();
}

GLTexture::GLTexture(const QImage& image, GLenum target)
{
    init();
    load(image, target);
}

GLTexture::GLTexture(const QPixmap& pixmap, GLenum target)
{
    init();
    load(pixmap, target);
}

GLTexture::GLTexture(const QString& fileName)
{
    init();
    load(fileName);
}

GLTexture::GLTexture(int width, int height)
{
    init();

    if (NPOTTextureSupported() || (isPowerOfTwo(width) && isPowerOfTwo(height))) {
        mTarget = GL_TEXTURE_2D;
        mScale.setWidth(1.0 / width);
        mScale.setHeight(1.0 / height);
        mSize = QSize(width, height);
        can_use_mipmaps = true;

        glGenTextures(1, &mTexture);
        bind();
#ifdef KWIN_HAVE_OPENGLES
        // format and internal format have to match in ES, GL_RGBA8 and GL_BGRA are not available
        // see http://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexImage2D.xml
        glTexImage2D(mTarget, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
#else
        glTexImage2D(mTarget, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
#endif
        unbind();
    }
}

GLTexture::~GLTexture()
{
    delete m_vbo;
    discard();
    assert(mUnnormalizeActive == 0);
    assert(mNormalizeActive == 0);
}

void GLTexture::init()
{
    mTexture = None;
    mTarget = 0;
    mFilter = 0;
    y_inverted = false;
    can_use_mipmaps = false;
    has_valid_mipmaps = false;
    mUnnormalizeActive = 0;
    mNormalizeActive = 0;
    m_vbo = 0;
}

void GLTexture::initStatic()
{
#ifdef KWIN_HAVE_OPENGLES
    sNPOTTextureSupported = true;
    sFramebufferObjectSupported = true;
    sSaturationSupported = true;
#else
    sNPOTTextureSupported = hasGLExtension("GL_ARB_texture_non_power_of_two");
    sFramebufferObjectSupported = hasGLExtension("GL_EXT_framebuffer_object");
    sSaturationSupported = ((hasGLExtension("GL_ARB_texture_env_crossbar")
                             && hasGLExtension("GL_ARB_texture_env_dot3")) || hasGLVersion(1, 4))
                           && (glTextureUnitsCount >= 4) && glActiveTexture != NULL;
#endif
}

bool GLTexture::isNull() const
{
    return mTexture == None;
}

QSize GLTexture::size() const
{
    return mSize;
}

bool GLTexture::load(const QImage& image, GLenum target)
{
    if (image.isNull())
        return false;
    QImage img = image;
    mTarget = target;
#ifndef KWIN_HAVE_OPENGLES
    if (mTarget != GL_TEXTURE_RECTANGLE_ARB) {
#endif
        if (!NPOTTextureSupported()
                && (!isPowerOfTwo(image.width()) || !isPowerOfTwo(image.height()))) {
            // non-rectangular target requires POT texture
            img = img.scaled(nearestPowerOfTwo(image.width()),
                             nearestPowerOfTwo(image.height()));
        }
        mScale.setWidth(1.0 / img.width());
        mScale.setHeight(1.0 / img.height());
        can_use_mipmaps = true;
#ifndef KWIN_HAVE_OPENGLES
    } else {
        mScale.setWidth(1.0);
        mScale.setHeight(1.0);
        can_use_mipmaps = false;
    }
#endif
    setFilter(GL_LINEAR);
    mSize = img.size();
    y_inverted = false;

    img = convertToGLFormat(img);

    setDirty();
    if (isNull())
        glGenTextures(1, &mTexture);
    bind();
#ifdef KWIN_HAVE_OPENGLES
    // format and internal format have to match in ES, GL_RGBA8 and GL_BGRA are not available
    // see http://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexImage2D.xml
    glTexImage2D(mTarget, 0, GL_RGBA, img.width(), img.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
#else
    glTexImage2D(mTarget, 0, GL_RGBA8, img.width(), img.height(), 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, img.bits());
#endif
    unbind();
    return true;
}

bool GLTexture::load(const QPixmap& pixmap, GLenum target)
{
    if (pixmap.isNull())
        return false;
    return load(pixmap.toImage(), target);
}

bool GLTexture::load(const QString& fileName)
{
    if (fileName.isEmpty())
        return false;
    return load(QImage(fileName));
}

void GLTexture::discard()
{
    setDirty();
    if (mTexture != None)
        glDeleteTextures(1, &mTexture);
    mTexture = None;
}

void GLTexture::bind()
{
#ifndef KWIN_HAVE_OPENGLES
    glEnable(mTarget);
#endif
    glBindTexture(mTarget, mTexture);
    enableFilter();
}

void GLTexture::unbind()
{
    glBindTexture(mTarget, 0);
#ifndef KWIN_HAVE_OPENGLES
    glDisable(mTarget);
#endif
}

void GLTexture::render(QRegion region, const QRect& rect)
{
    if (rect.size() != m_cachedSize) {
        m_cachedSize = rect.size();
        QRect r(rect);
        r.moveTo(0, 0);
        if (!m_vbo) {
            m_vbo = new GLVertexBuffer(KWin::GLVertexBuffer::Static);
        }
        const float verts[ 4 * 2 ] = {
            // NOTICE: r.x/y could be replaced by "0", but that would make it unreadable...
            r.x(), r.y(),
            r.x(), r.y() + rect.height(),
            r.x() + rect.width(), r.y(),
            r.x() + rect.width(), r.y() + rect.height()
        };
        const float texcoords[ 4 * 2 ] = {
            0.0f, y_inverted ? 0.0f : 1.0f, // y needs to be swapped (normalized coords)
            0.0f, y_inverted ? 1.0f : 0.0f,
            1.0f, y_inverted ? 0.0f : 1.0f,
            1.0f, y_inverted ? 1.0f : 0.0f
        };
        m_vbo->setData(4, 2, verts, texcoords);
    }
    QMatrix4x4 translation;
    translation.translate(rect.x(), rect.y());
    if (ShaderManager::instance()->isShaderBound()) {
        GLShader *shader = ShaderManager::instance()->getBoundShader();
        shader->setUniform(GLShader::Offset, QVector2D(rect.x(), rect.y()));
        shader->setUniform(GLShader::WindowTransformation, translation);
    } else {
        pushMatrix(translation);
    }
    m_vbo->render(region, GL_TRIANGLE_STRIP);
    if (ShaderManager::instance()->isShaderBound()) {
        GLShader *shader = ShaderManager::instance()->getBoundShader();
        shader->setUniform(GLShader::WindowTransformation, QMatrix4x4());
    } else {
        popMatrix();
    }
}

void GLTexture::enableUnnormalizedTexCoords()
{
#ifndef KWIN_HAVE_OPENGLES
    assert(mNormalizeActive == 0);
    if (mUnnormalizeActive++ != 0)
        return;
    // update texture matrix to handle GL_TEXTURE_2D and GL_TEXTURE_RECTANGLE
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glScalef(mScale.width(), mScale.height(), 1);
    if (!y_inverted) {
        // Modify texture matrix so that we could always use non-opengl
        //  coordinates for textures
        glScalef(1, -1, 1);
        glTranslatef(0, -mSize.height(), 0);
    }
    glMatrixMode(GL_MODELVIEW);
#endif
}

void GLTexture::disableUnnormalizedTexCoords()
{
#ifndef KWIN_HAVE_OPENGLES
    if (--mUnnormalizeActive != 0)
        return;
    // Restore texture matrix
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
#endif
}

void GLTexture::enableNormalizedTexCoords()
{
#ifndef KWIN_HAVE_OPENGLES
    assert(mUnnormalizeActive == 0);
    if (mNormalizeActive++ != 0)
        return;
    // update texture matrix to handle GL_TEXTURE_2D and GL_TEXTURE_RECTANGLE
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glScalef(mSize.width() * mScale.width(), mSize.height() * mScale.height(), 1);
    if (y_inverted) {
        // Modify texture matrix so that we could always use non-opengl
        //  coordinates for textures
        glScalef(1, -1, 1);
        glTranslatef(0, -1, 0);
    }
    glMatrixMode(GL_MODELVIEW);
#endif
}

void GLTexture::disableNormalizedTexCoords()
{
#ifndef KWIN_HAVE_OPENGLES
    if (--mNormalizeActive != 0)
        return;
    // Restore texture matrix
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
#endif
}

GLuint GLTexture::texture() const
{
    return mTexture;
}

GLenum GLTexture::target() const
{
    return mTarget;
}

GLenum GLTexture::filter() const
{
    return mFilter;
}

bool GLTexture::isDirty() const
{
    return has_valid_mipmaps;
}

void GLTexture::setTexture(GLuint texture)
{
    discard();
    mTexture = texture;
}

void GLTexture::setTarget(GLenum target)
{
    mTarget = target;
}

void GLTexture::setFilter(GLenum filter)
{
    mFilter = filter;
}

void GLTexture::setWrapMode(GLenum mode)
{
    bind();
    glTexParameteri(mTarget, GL_TEXTURE_WRAP_S, mode);
    glTexParameteri(mTarget, GL_TEXTURE_WRAP_T, mode);
    unbind();
}

void GLTexture::setDirty()
{
    has_valid_mipmaps = false;
}


void GLTexture::enableFilter()
{
    if (mFilter == GL_LINEAR_MIPMAP_LINEAR) {
        // trilinear filtering requested, but is it possible?
        if (NPOTTextureSupported()
                && framebufferObjectSupported()
                && can_use_mipmaps) {
            glTexParameteri(mTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(mTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            if (!has_valid_mipmaps) {
                glGenerateMipmap(mTarget);
                has_valid_mipmaps = true;
            }
        } else {
            // can't use trilinear, so use bilinear
            setFilter(GL_LINEAR);
            glTexParameteri(mTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(mTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
    } else if (mFilter == GL_LINEAR) {
        glTexParameteri(mTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(mTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        // if neither trilinear nor bilinear, default to fast filtering
        setFilter(GL_NEAREST);
        glTexParameteri(mTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(mTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
}

static void convertToGLFormatHelper(QImage &dst, const QImage &img, GLenum texture_format)
{
#ifdef KWIN_HAVE_OPENGLES
    Q_UNUSED(texture_format)
#endif
    // Copied from Qt
    Q_ASSERT(dst.size() == img.size());
    Q_ASSERT(dst.depth() == 32);
    Q_ASSERT(img.depth() == 32);

    const int width = img.width();
    const int height = img.height();
    const uint *p = (const uint*) img.scanLine(img.height() - 1);
    uint *q = (uint*) dst.scanLine(0);

#ifndef KWIN_HAVE_OPENGLES
    if (texture_format == GL_BGRA) {
        if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
            // mirror + swizzle
            for (int i = 0; i < height; ++i) {
                const uint *end = p + width;
                while (p < end) {
                    *q = ((*p << 24) & 0xff000000)
                         | ((*p >> 24) & 0x000000ff)
                         | ((*p << 8) & 0x00ff0000)
                         | ((*p >> 8) & 0x0000ff00);
                    p++;
                    q++;
                }
                p -= 2 * width;
            }
        } else {
            const uint bytesPerLine = img.bytesPerLine();
            for (int i = 0; i < height; ++i) {
                memcpy(q, p, bytesPerLine);
                q += width;
                p -= width;
            }
        }
    } else {
#endif
        if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
            for (int i = 0; i < height; ++i) {
                const uint *end = p + width;
                while (p < end) {
                    *q = (*p << 8) | ((*p >> 24) & 0xFF);
                    p++;
                    q++;
                }
                p -= 2 * width;
            }
        } else {
            for (int i = 0; i < height; ++i) {
                const uint *end = p + width;
                while (p < end) {
                    *q = ((*p << 16) & 0xff0000) | ((*p >> 16) & 0xff) | (*p & 0xff00ff00);
                    p++;
                    q++;
                }
                p -= 2 * width;
            }
        }
#ifndef KWIN_HAVE_OPENGLES
    }
#endif
}

QImage GLTexture::convertToGLFormat(const QImage& img) const
{
    // Copied from Qt's QGLWidget::convertToGLFormat()
    QImage res(img.size(), QImage::Format_ARGB32);
#ifdef KWIN_HAVE_OPENGLES
    convertToGLFormatHelper(res, img.convertToFormat(QImage::Format_ARGB32_Premultiplied), GL_RGBA);
#else
    convertToGLFormatHelper(res, img.convertToFormat(QImage::Format_ARGB32_Premultiplied), GL_BGRA);
#endif
    return res;
}

} // namespace KWin

#endif
