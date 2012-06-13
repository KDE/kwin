/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006-2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2012 Philipp Knechtges <philipp-dev@knechtges.com>

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

#include "kwinglplatform.h"
#include "kwineffects.h"
#include "kwinglutils_funcs.h"
#include "kwinglutils.h"

#include "kwingltexture_p.h"

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

bool GLTexturePrivate::sNPOTTextureSupported = false;
bool GLTexturePrivate::sFramebufferObjectSupported = false;
bool GLTexturePrivate::sSaturationSupported = false;

GLTexture::GLTexture()
    : d_ptr(new GLTexturePrivate())
{
}

GLTexture::GLTexture(GLTexturePrivate& dd)
    : d_ptr(&dd)
{
}

GLTexture::GLTexture(const GLTexture& tex)
    : d_ptr(tex.d_ptr)
{
}

GLTexture::GLTexture(const QImage& image, GLenum target)
    : d_ptr(new GLTexturePrivate())
{
    load(image, target);
}

GLTexture::GLTexture(const QPixmap& pixmap, GLenum target)
    : d_ptr(new GLTexturePrivate())
{
    load(pixmap, target);
}

GLTexture::GLTexture(const QString& fileName)
     : d_ptr(new GLTexturePrivate())
{
    load(fileName);
}

GLTexture::GLTexture(int width, int height)
     : d_ptr(new GLTexturePrivate())
{
    Q_D(GLTexture);
    if (NPOTTextureSupported() || (isPowerOfTwo(width) && isPowerOfTwo(height))) {
        d->m_target = GL_TEXTURE_2D;
        d->m_scale.setWidth(1.0 / width);
        d->m_scale.setHeight(1.0 / height);
        d->m_size = QSize(width, height);
        d->m_canUseMipmaps = true;

        glGenTextures(1, &d->m_texture);
        bind();
#ifdef KWIN_HAVE_OPENGLES
        // format and internal format have to match in ES, GL_RGBA8 and GL_BGRA are not available
        // see http://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexImage2D.xml
        glTexImage2D(d->m_target, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
#else
        glTexImage2D(d->m_target, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
#endif
        unbind();
    }
}

GLTexture::~GLTexture()
{
}

GLTexture& GLTexture::operator = (const GLTexture& tex)
{
    d_ptr = tex.d_ptr;
    return *this;
}

GLTexturePrivate::GLTexturePrivate()
{
    m_texture = 0;
    m_target = 0;
    m_filter = GL_NEAREST;
    m_wrapMode = GL_REPEAT;
    m_yInverted = false;
    m_canUseMipmaps = false;
    m_markedDirty = false;
    m_unnormalizeActive = 0;
    m_normalizeActive = 0;
    m_vbo = 0;
    m_filterChanged = true;
    m_wrapModeChanged = false;
}

GLTexturePrivate::~GLTexturePrivate()
{
    if (m_vbo != 0) {
        delete m_vbo;
    }
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
    }
}

void GLTexturePrivate::initStatic()
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
    Q_D(const GLTexture);
    return None == d->m_texture;
}

QSize GLTexture::size() const
{
    Q_D(const GLTexture);
    return d->m_size;
}

bool GLTexture::load(const QImage& image, GLenum target)
{
    // decrease the reference counter for the old texture
    d_ptr = new GLTexturePrivate();

    Q_D(GLTexture);
    if (image.isNull())
        return false;
    QImage img = image;
    d->m_target = target;
#ifndef KWIN_HAVE_OPENGLES
    if (d->m_target != GL_TEXTURE_RECTANGLE_ARB) {
#endif
        if (!NPOTTextureSupported()
                && (!isPowerOfTwo(image.width()) || !isPowerOfTwo(image.height()))) {
            // non-rectangular target requires POT texture
            img = img.scaled(nearestPowerOfTwo(image.width()),
                             nearestPowerOfTwo(image.height()));
        }
        d->m_scale.setWidth(1.0 / img.width());
        d->m_scale.setHeight(1.0 / img.height());
        d->m_canUseMipmaps = true;
#ifndef KWIN_HAVE_OPENGLES
    } else {
        d->m_scale.setWidth(1.0);
        d->m_scale.setHeight(1.0);
        d->m_canUseMipmaps = false;
    }
#endif
    d->m_size = img.size();
    d->m_yInverted = true;

    img = d->convertToGLFormat(img);

    if (isNull()) {
        glGenTextures(1, &d->m_texture);
    }
    bind();
#ifdef KWIN_HAVE_OPENGLES
    // format and internal format have to match in ES, GL_RGBA8 and GL_BGRA are not available
    // see http://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexImage2D.xml
    glTexImage2D(d->m_target, 0, GL_RGBA, img.width(), img.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
#else
    glTexImage2D(d->m_target, 0, GL_RGBA8, img.width(), img.height(), 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, img.bits());
#endif
    unbind();
    setFilter(GL_LINEAR);
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
    d_ptr = new GLTexturePrivate();
}

void GLTexturePrivate::bind()
{
#ifndef KWIN_HAVE_OPENGLES
    glEnable(m_target);
#endif
    glBindTexture(m_target, m_texture);
}

void GLTexture::bind()
{
    Q_D(GLTexture);
    d->bind();
    if (d->m_markedDirty) {
        d->onDamage();
    }
    if (d->m_filterChanged) {
        if (d->m_filter == GL_LINEAR_MIPMAP_LINEAR) {
            // trilinear filtering requested, but is it possible?
            if (d->sNPOTTextureSupported
                    && d->sFramebufferObjectSupported
                    && d->m_canUseMipmaps) {
                glTexParameteri(d->m_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(d->m_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glGenerateMipmap(d->m_target);
            } else {
                // can't use trilinear, so use bilinear
                d->m_filter = GL_LINEAR;
                glTexParameteri(d->m_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(d->m_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }
        } else if (d->m_filter == GL_LINEAR) {
            glTexParameteri(d->m_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(d->m_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        } else {
            // if neither trilinear nor bilinear, default to fast filtering
            d->m_filter = GL_NEAREST;
            glTexParameteri(d->m_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(d->m_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
    }
    if (d->m_wrapModeChanged) {
        glTexParameteri(d->m_target, GL_TEXTURE_WRAP_S, d->m_wrapMode);
        glTexParameteri(d->m_target, GL_TEXTURE_WRAP_T, d->m_wrapMode);
    }
}

void GLTexturePrivate::unbind()
{
    glBindTexture(m_target, 0);
#ifndef KWIN_HAVE_OPENGLES
    glDisable(m_target);
#endif
}

void GLTexture::unbind()
{
    Q_D(GLTexture);
    d->unbind();
}

void GLTexture::render(QRegion region, const QRect& rect, bool hardwareClipping)
{
    Q_D(GLTexture);
    if (rect.size() != d->m_cachedSize) {
        d->m_cachedSize = rect.size();
        QRect r(rect);
        r.moveTo(0, 0);
        if (!d->m_vbo) {
            d->m_vbo = new GLVertexBuffer(KWin::GLVertexBuffer::Static);
        }
        const float verts[ 4 * 2 ] = {
            // NOTICE: r.x/y could be replaced by "0", but that would make it unreadable...
            r.x(), r.y(),
            r.x(), r.y() + rect.height(),
            r.x() + rect.width(), r.y(),
            r.x() + rect.width(), r.y() + rect.height()
        };
#ifdef KWIN_HAVE_OPENGLES
        const float texWidth = 1.0f;
        const float texHeight = 1.0f;
#else
        const float texWidth = (target() == GL_TEXTURE_RECTANGLE_ARB) ? width() : 1.0f;
        const float texHeight = (target() == GL_TEXTURE_RECTANGLE_ARB) ? height() : 1.0f;
#endif
        const float texcoords[ 4 * 2 ] = {
            0.0f, d->m_yInverted ? 0.0f : texHeight, // y needs to be swapped (normalized coords)
            0.0f, d->m_yInverted ? texHeight : 0.0f,
            texWidth, d->m_yInverted ? 0.0f : texHeight,
            texWidth, d->m_yInverted ? texHeight : 0.0f
        };
        d->m_vbo->setData(4, 2, verts, texcoords);
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
    d->m_vbo->render(region, GL_TRIANGLE_STRIP, hardwareClipping);
    if (ShaderManager::instance()->isShaderBound()) {
        GLShader *shader = ShaderManager::instance()->getBoundShader();
        shader->setUniform(GLShader::WindowTransformation, QMatrix4x4());
    } else {
        popMatrix();
    }
}

GLuint GLTexture::texture() const
{
    Q_D(const GLTexture);
    return d->m_texture;
}

GLenum GLTexture::target() const
{
    Q_D(const GLTexture);
    return d->m_target;
}

GLenum GLTexture::filter() const
{
    Q_D(const GLTexture);
    return d->m_filter;
}

bool GLTexture::isDirty() const
{
    Q_D(const GLTexture);
    return d->m_markedDirty;
}

void GLTexture::setFilter(GLenum filter)
{
    Q_D(GLTexture);
    if (filter != d->m_filter) {
        d->m_filter = filter;
        d->m_filterChanged = true;
    }
}

void GLTexture::setWrapMode(GLenum mode)
{
    Q_D(GLTexture);
    if (mode != d->m_wrapMode) {
        d->m_wrapMode = mode;
        d->m_wrapModeChanged=true;
    }
}

void GLTexturePrivate::onDamage()
{
    if (m_filter == GL_LINEAR_MIPMAP_LINEAR && !m_filterChanged) {
        glGenerateMipmap(m_target);
    }
}

void GLTexture::setDirty()
{
    Q_D(GLTexture);
    d->m_markedDirty = true;
}

QImage GLTexturePrivate::convertToGLFormat(const QImage& img) const
{
    // Copied from Qt's QGLWidget::convertToGLFormat()
    QImage res;
#ifdef KWIN_HAVE_OPENGLES
    res = QImage(img.size(), QImage::Format_ARGB32);
    QImage imgARGB32 = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    const int width = img.width();
    const int height = img.height();
    const uint32_t *p = (const uint32_t*) imgARGB32.scanLine(0);
    uint32_t *q = (uint32_t*) res.scanLine(0);

    if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        for (int i = 0; i < height; ++i) {
            const uint32_t *end = p + width;
            while (p < end) {
                *q = (*p << 8) | ((*p >> 24) & 0xFF);
                p++;
                q++;
            }
        }
    } else {
        // GL_BGRA -> GL_RGBA
        for (int i = 0; i < height; ++i) {
            const uint32_t *end = p + width;
            while (p < end) {
                *q = ((*p << 16) & 0xff0000) | ((*p >> 16) & 0xff) | (*p & 0xff00ff00);
                p++;
                q++;
            }
        }
    }
#else
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        res = QImage(img.size(), QImage::Format_ARGB32);
        QImage imgARGB32 = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);

        const int width = img.width();
        const int height = img.height();
        const uint32_t *p = (const uint32_t*) imgARGB32.scanLine(0);
        uint32_t *q = (uint32_t*) res.scanLine(0);

        // swizzle
        for (int i = 0; i < height; ++i) {
            const uint32_t *end = p + width;
            while (p < end) {
                *q = ((*p << 24) & 0xff000000)
                     | ((*p >> 24) & 0x000000ff)
                     | ((*p << 8) & 0x00ff0000)
                     | ((*p >> 8) & 0x0000ff00);
                p++;
                q++;
            }
        }
    } else {
        res = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
#endif
    return res;
}

bool GLTexture::isYInverted() const
{
    Q_D(const GLTexture);
    return d->m_yInverted;
}

void GLTexture::setYInverted(bool inverted)
{
    Q_D(GLTexture);
    d->m_yInverted = inverted;
}

int GLTexture::width() const
{
    Q_D(const GLTexture);
    return d->m_size.width();
}

int GLTexture::height() const
{
    Q_D(const GLTexture);
    return d->m_size.height();
}

bool GLTexture::NPOTTextureSupported()
{
    return GLTexturePrivate::sNPOTTextureSupported;
}

bool GLTexture::framebufferObjectSupported()
{
    return GLTexturePrivate::sFramebufferObjectSupported;
}

bool GLTexture::saturationSupported()
{
    return GLTexturePrivate::sSaturationSupported;
}

} // namespace KWin
