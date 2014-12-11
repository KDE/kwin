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

bool GLTexturePrivate::s_supportsFramebufferObjects = false;
bool GLTexturePrivate::s_supportsARGB32 = false;
bool GLTexturePrivate::s_supportsUnpack = false;
uint GLTexturePrivate::s_textureObjectCounter = 0;
uint GLTexturePrivate::s_fbo = 0;


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
    Q_D(GLTexture);

    if (image.isNull())
        return;

    d->m_target = target;

    if (d->m_target != GL_TEXTURE_RECTANGLE_ARB) {
        d->m_scale.setWidth(1.0 / image.width());
        d->m_scale.setHeight(1.0 / image.height());
        d->m_canUseMipmaps = true;
    } else {
        d->m_scale.setWidth(1.0);
        d->m_scale.setHeight(1.0);
        d->m_canUseMipmaps = false;
    }

    d->m_size = image.size();
    d->m_yInverted = true;

    d->updateMatrix();

    glGenTextures(1, &d->m_texture);
    bind();

    if (!GLPlatform::instance()->isGLES()) {
        const QImage im = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        glTexImage2D(d->m_target, 0, GL_RGBA8, im.width(), im.height(), 0,
                     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, im.bits());
    } else {
        if (d->s_supportsARGB32) {
            const QImage im = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            glTexImage2D(d->m_target, 0, GL_BGRA_EXT, im.width(), im.height(),
                         0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, im.bits());
        } else {
            const QImage im = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            glTexImage2D(d->m_target, 0, GL_RGBA, im.width(), im.height(),
                         0, GL_RGBA, GL_UNSIGNED_BYTE, im.bits());
        }
    }

    unbind();
    setFilter(GL_LINEAR);
}

GLTexture::GLTexture(const QPixmap& pixmap, GLenum target)
    : GLTexture(pixmap.toImage(), target)
{
}

GLTexture::GLTexture(const QString& fileName)
     : GLTexture(QImage(fileName))
{
}

GLTexture::GLTexture(int width, int height)
     : d_ptr(new GLTexturePrivate())
{
    Q_D(GLTexture);

    d->m_target = GL_TEXTURE_2D;
    d->m_scale.setWidth(1.0 / width);
    d->m_scale.setHeight(1.0 / height);
    d->m_size = QSize(width, height);
    d->m_canUseMipmaps = true;

    d->updateMatrix();

    glGenTextures(1, &d->m_texture);
    bind();

    if (!GLPlatform::instance()->isGLES()) {
        glTexImage2D(d->m_target, 0, GL_RGBA8, width, height, 0,
                     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, nullptr);
    } else {
        // The format parameter in glTexSubImage() must match the internal format
        // of the texture, so it's important that we allocate the texture with
        // the format that will be used in update() and clear().
        const GLenum format = d->s_supportsARGB32 ? GL_BGRA_EXT : GL_RGBA;
        glTexImage2D(d->m_target, 0, format, width, height, 0,
                     format, GL_UNSIGNED_BYTE, nullptr);
    }

    unbind();
}

GLTexture::GLTexture(const QSize &size)
    : GLTexture(size.width(), size.height())
{
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
    m_vbo = nullptr;
    m_filterChanged = true;
    m_wrapModeChanged = false;
    ++s_textureObjectCounter;
}

GLTexturePrivate::~GLTexturePrivate()
{
    if (m_vbo != nullptr) {
        delete m_vbo;
    }
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
    }
    // Delete the FBO if this is the last Texture
    if (--s_textureObjectCounter == 0 && s_fbo) {
        glDeleteFramebuffers(1, &s_fbo);
        s_fbo = 0;
    }
}

void GLTexturePrivate::initStatic()
{
    if (!GLPlatform::instance()->isGLES()) {
        s_supportsFramebufferObjects = hasGLVersion(3, 0) ||
            hasGLExtension("GL_ARB_framebuffer_object") || hasGLExtension(QByteArrayLiteral("GL_EXT_framebuffer_object"));
        s_supportsARGB32 = true;
        s_supportsUnpack = true;
    } else {
        s_supportsFramebufferObjects = true;

        // QImage::Format_ARGB32_Premultiplied is a packed-pixel format, so it's only
        // equivalent to GL_BGRA/GL_UNSIGNED_BYTE on little-endian systems.
        s_supportsARGB32 = QSysInfo::ByteOrder == QSysInfo::LittleEndian &&
            hasGLExtension(QByteArrayLiteral("GL_EXT_texture_format_BGRA8888"));

        s_supportsUnpack = hasGLExtension(QByteArrayLiteral("GL_EXT_unpack_subimage"));
    }
}

void GLTexturePrivate::cleanup()
{
    s_supportsFramebufferObjects = false;
    s_supportsARGB32 = false;
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

void GLTexture::update(const QImage &image, const QPoint &offset, const QRect &src)
{
    if (image.isNull() || isNull())
        return;

    Q_D(GLTexture);

    bool useUnpack = !src.isNull() && d->s_supportsUnpack && d->s_supportsARGB32 && image.format() == QImage::Format_ARGB32_Premultiplied;

    int width = image.width();
    int height = image.height();
    QImage tmpImage;

    if (!src.isNull()) {
        if (d->s_supportsUnpack) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, image.width());
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, src.x());
            glPixelStorei(GL_UNPACK_SKIP_ROWS, src.y());
        } else {
            tmpImage = image.copy(src);
        }
        width = src.width();
        height = src.height();
    }

    const QImage &img = tmpImage.isNull() ? image : tmpImage;

    bind();

    if (!GLPlatform::instance()->isGLES()) {
        const QImage im = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        glTexSubImage2D(d->m_target, 0, offset.x(), offset.y(), width, height,
                        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, im.bits());
    } else {
        if (d->s_supportsARGB32) {
            const QImage im = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            glTexSubImage2D(d->m_target, 0, offset.x(), offset.y(), width, height,
                            GL_BGRA_EXT, GL_UNSIGNED_BYTE, im.bits());
        } else {
            const QImage im = img.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            glTexSubImage2D(d->m_target, 0, offset.x(), offset.y(), width, height,
                            GL_RGBA, GL_UNSIGNED_BYTE, im.bits());
        }
    }

    unbind();

    if (useUnpack) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    }
}

void GLTexture::discard()
{
    d_ptr = new GLTexturePrivate();
}

void GLTexture::bind()
{
    Q_D(GLTexture);

    glBindTexture(d->m_target, d->m_texture);

    if (d->m_markedDirty) {
        d->onDamage();
    }
    if (d->m_filterChanged) {
        if (d->m_filter == GL_LINEAR_MIPMAP_LINEAR) {
            // trilinear filtering requested, but is it possible?
            if (d->s_supportsFramebufferObjects && d->m_canUseMipmaps) {
                glTexParameteri(d->m_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(d->m_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
        d->m_filterChanged = false;
    }
    if (d->m_wrapModeChanged) {
        glTexParameteri(d->m_target, GL_TEXTURE_WRAP_S, d->m_wrapMode);
        glTexParameteri(d->m_target, GL_TEXTURE_WRAP_T, d->m_wrapMode);
        d->m_wrapModeChanged = false;
    }
}

void GLTexture::generateMipmaps()
{
    Q_D(GLTexture);

    if (d->m_canUseMipmaps && d->s_supportsFramebufferObjects)
        glGenerateMipmap(d->m_target);
}

void GLTexture::unbind()
{
    Q_D(GLTexture);
    glBindTexture(d->m_target, 0);
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
            static_cast<float>(r.x()), static_cast<float>(r.y()),
            static_cast<float>(r.x()), static_cast<float>(r.y() + rect.height()),
            static_cast<float>(r.x() + rect.width()), static_cast<float>(r.y()),
            static_cast<float>(r.x() + rect.width()), static_cast<float>(r.y() + rect.height())
        };

        const float texWidth = (target() == GL_TEXTURE_RECTANGLE_ARB) ? width() : 1.0f;
        const float texHeight = (target() == GL_TEXTURE_RECTANGLE_ARB) ? height() : 1.0f;

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
    GLShader *shader = ShaderManager::instance()->getBoundShader();
    shader->setUniform(GLShader::Offset, QVector2D(rect.x(), rect.y()));
    shader->setUniform(GLShader::WindowTransformation, translation);
    d->m_vbo->render(region, GL_TRIANGLE_STRIP, hardwareClipping);
    shader->setUniform(GLShader::WindowTransformation, QMatrix4x4());
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

void GLTexture::clear()
{
    Q_D(GLTexture);
    if (!GLTexturePrivate::s_fbo && GLRenderTarget::supported() &&
        GLPlatform::instance()->driver() != Driver_Catalyst) // fail. -> bug #323065
        glGenFramebuffers(1, &GLTexturePrivate::s_fbo);

    if (GLTexturePrivate::s_fbo) {
        // Clear the texture
        glBindFramebuffer(GL_FRAMEBUFFER, GLTexturePrivate::s_fbo);
        glClearColor(0, 0, 0, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, d->m_texture, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    } else {
        if (const int size = width()*height()) {
            uint32_t *buffer = new uint32_t[size];
            memset(buffer, 0, size*sizeof(uint32_t));
            bind();
            if (!GLPlatform::instance()->isGLES()) {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width(), height(),
                                GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer);
            } else {
                const GLenum format = d->s_supportsARGB32 ? GL_BGRA_EXT : GL_RGBA;
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width(), height(),
                                format, GL_UNSIGNED_BYTE, buffer);
            }
            unbind();
            delete[] buffer;
        }
    }
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
    // No-op
}

void GLTexture::setDirty()
{
    Q_D(GLTexture);
    d->m_markedDirty = true;
}

void GLTexturePrivate::updateMatrix()
{
    m_matrix[NormalizedCoordinates].setToIdentity();
    m_matrix[UnnormalizedCoordinates].setToIdentity();

    if (m_target == GL_TEXTURE_RECTANGLE_ARB)
        m_matrix[NormalizedCoordinates].scale(m_size.width(), m_size.height());
    else
        m_matrix[UnnormalizedCoordinates].scale(1.0 / m_size.width(), 1.0 / m_size.height());

    if (!m_yInverted) {
        m_matrix[NormalizedCoordinates].translate(0.0, 1.0);
        m_matrix[NormalizedCoordinates].scale(1.0, -1.0);

        m_matrix[UnnormalizedCoordinates].translate(0.0, m_size.height());
        m_matrix[UnnormalizedCoordinates].scale(1.0, -1.0);
    }
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
    d->updateMatrix();
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

QMatrix4x4 GLTexture::matrix(TextureCoordinateType type) const
{
    Q_D(const GLTexture);
    return d->m_matrix[type];
}

bool GLTexture::framebufferObjectSupported()
{
    return GLTexturePrivate::s_supportsFramebufferObjects;
}

} // namespace KWin
