/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2012 Philipp Knechtges <philipp-dev@knechtges.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gltexture_p.h"
#include "opengl/glframebuffer.h"
#include "opengl/glplatform.h"
#include "opengl/glutils.h"
#include "utils/common.h"

#include <QImage>
#include <QPixmap>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

namespace KWin
{

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
} static const formatTable[] = {
    {0, 0, 0}, // QImage::Format_Invalid
    {0, 0, 0}, // QImage::Format_Mono
    {0, 0, 0}, // QImage::Format_MonoLSB
    {0, 0, 0}, // QImage::Format_Indexed8
    {GL_RGB8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV}, // QImage::Format_RGB32
    {0, 0, 0}, // QImage::Format_ARGB32
    {GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV}, // QImage::Format_ARGB32_Premultiplied
    {GL_RGB8, GL_RGB, GL_UNSIGNED_SHORT_5_6_5}, // QImage::Format_RGB16
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

GLTexture::GLTexture(GLenum target)
    : d(std::make_unique<GLTexturePrivate>())
{
    d->m_target = target;
}

GLTexture::GLTexture(GLenum target, GLuint textureId, GLenum internalFormat, const QSize &size, int levels, bool owning, OutputTransform transform)
    : GLTexture(target)
{
    d->m_owning = owning;
    d->m_texture = textureId;
    d->m_scale.setWidth(1.0 / size.width());
    d->m_scale.setHeight(1.0 / size.height());
    d->m_size = size;
    d->m_canUseMipmaps = levels > 1;
    d->m_mipLevels = levels;
    d->m_filter = levels > 1 ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST;
    d->m_internalFormat = internalFormat;
    d->m_textureToBufferTransform = transform;

    d->updateMatrix();
}

GLTexture::~GLTexture()
{
}

bool GLTexture::create()
{
    if (!isNull()) {
        return true;
    }
    glGenTextures(1, &d->m_texture);
    return d->m_texture != GL_NONE;
}

GLTexturePrivate::GLTexturePrivate()
    : m_texture(0)
    , m_target(0)
    , m_internalFormat(0)
    , m_filter(GL_NEAREST)
    , m_wrapMode(GL_REPEAT)
    , m_canUseMipmaps(false)
    , m_markedDirty(false)
    , m_filterChanged(true)
    , m_wrapModeChanged(false)
    , m_owning(true)
    , m_mipLevels(1)
    , m_unnormalizeActive(0)
    , m_normalizeActive(0)
{
}

GLTexturePrivate::~GLTexturePrivate()
{
    if (!OpenGlContext::currentContext()) {
        qCWarning(KWIN_OPENGL, "Could not delete texture because no context is current");
        return;
    }
    if (m_texture != 0 && m_owning) {
        glDeleteTextures(1, &m_texture);
    }
}

bool GLTexture::isNull() const
{
    return GL_NONE == d->m_texture;
}

QSize GLTexture::size() const
{
    return d->m_size;
}

void GLTexture::setSize(const QSize &size)
{
    if (!isNull()) {
        return;
    }
    d->m_size = size;
    d->updateMatrix();
}

void GLTexture::update(const QImage &image, const QRegion &region, const QPoint &offset)
{
    if (image.isNull() || isNull()) {
        return;
    }

    Q_ASSERT(d->m_owning);

    const auto context = OpenGlContext::currentContext();
    GLenum glFormat;
    GLenum type;
    QImage::Format uploadFormat;
    if (!context->isOpenGLES()) {
        const QImage::Format index = image.format();

        if (index < sizeof(formatTable) / sizeof(formatTable[0]) && formatTable[index].internalFormat) {
            glFormat = formatTable[index].format;
            type = formatTable[index].type;
            uploadFormat = index;
        } else {
            glFormat = GL_BGRA;
            type = GL_UNSIGNED_INT_8_8_8_8_REV;
            uploadFormat = QImage::Format_ARGB32_Premultiplied;
        }
    } else {
        if (context->supportsARGB32Textures()) {
            glFormat = GL_BGRA_EXT;
            type = GL_UNSIGNED_BYTE;
            uploadFormat = QImage::Format_ARGB32_Premultiplied;
        } else {
            glFormat = GL_RGBA;
            type = GL_UNSIGNED_BYTE;
            uploadFormat = QImage::Format_RGBA8888_Premultiplied;
        }
    }

    QImage im = image;
    if (im.format() != uploadFormat) {
        im.convertTo(uploadFormat);
    }

    bind();

    for (const QRect &rect : region) {
        Q_ASSERT(im.depth() % 8 == 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, im.bytesPerLine() / (im.depth() / 8));
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, rect.x());
        glPixelStorei(GL_UNPACK_SKIP_ROWS, rect.y());

        glTexSubImage2D(d->m_target, 0, offset.x() + rect.x(), offset.y() + rect.y(), rect.width(), rect.height(), glFormat, type, im.constBits());
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    unbind();
}

void GLTexture::bind()
{
    Q_ASSERT(d->m_texture);

    glBindTexture(d->m_target, d->m_texture);

    if (d->m_markedDirty) {
        onDamage();
    }
    if (d->m_filterChanged) {
        GLenum minFilter = GL_NEAREST;
        GLenum magFilter = GL_NEAREST;

        switch (d->m_filter) {
        case GL_NEAREST:
            minFilter = magFilter = GL_NEAREST;
            break;

        case GL_LINEAR:
            minFilter = magFilter = GL_LINEAR;
            break;

        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_NEAREST_MIPMAP_LINEAR:
            magFilter = GL_NEAREST;
            minFilter = d->m_canUseMipmaps ? d->m_filter : GL_NEAREST;
            break;

        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_LINEAR:
            magFilter = GL_LINEAR;
            minFilter = d->m_canUseMipmaps ? d->m_filter : GL_LINEAR;
            break;
        }

        glTexParameteri(d->m_target, GL_TEXTURE_MIN_FILTER, minFilter);
        glTexParameteri(d->m_target, GL_TEXTURE_MAG_FILTER, magFilter);

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
    if (d->m_canUseMipmaps) {
        glGenerateMipmap(d->m_target);
    }
}

void GLTexture::unbind()
{
    glBindTexture(d->m_target, 0);
}

void GLTexture::render(const QSizeF &size)
{
    render(infiniteRegion(), size, false);
}

void GLTexture::render(const QRegion &region, const QSizeF &targetSize, bool hardwareClipping)
{
    const auto rotatedSize = d->m_textureToBufferTransform.map(size());
    render(QRectF(QPoint(), rotatedSize), region, targetSize, hardwareClipping);
}

void GLTexture::render(const QRectF &source, const QRegion &region, const QSizeF &targetSize, bool hardwareClipping)
{
    if (targetSize.isEmpty()) {
        return; // nothing to paint and m_vbo is likely nullptr and d->m_cachedSize empty as well, #337090
    }

    const QSize destinationSize = targetSize.toSize(); // TODO: toSize is not enough to snap to the pixel grid, fix render() users and drop this toSize
    if (destinationSize != d->m_cachedSize || d->m_cachedSource != source || d->m_cachedContentTransform != d->m_textureToBufferTransform) {
        d->m_cachedSize = destinationSize;
        d->m_cachedSource = source;
        d->m_cachedContentTransform = d->m_textureToBufferTransform;

        const float texWidth = (target() == GL_TEXTURE_RECTANGLE_ARB) ? width() : 1.0f;
        const float texHeight = (target() == GL_TEXTURE_RECTANGLE_ARB) ? height() : 1.0f;

        const QSize rotatedSize = d->m_textureToBufferTransform.map(size());

        QMatrix4x4 textureMat;
        textureMat.translate(texWidth / 2, texHeight / 2);
        // our Y axis is flipped vs OpenGL
        textureMat.scale(1, -1);
        textureMat *= d->m_textureToBufferTransform.toMatrix();
        textureMat.translate(-texWidth / 2, -texHeight / 2);
        textureMat.scale(texWidth / rotatedSize.width(), texHeight / rotatedSize.height());

        const QPointF p1 = textureMat.map(QPointF(source.x(), source.y()));
        const QPointF p2 = textureMat.map(QPointF(source.x(), source.y() + source.height()));
        const QPointF p3 = textureMat.map(QPointF(source.x() + source.width(), source.y()));
        const QPointF p4 = textureMat.map(QPointF(source.x() + source.width(), source.y() + source.height()));

        if (!d->m_vbo) {
            d->m_vbo = std::make_unique<GLVertexBuffer>(KWin::GLVertexBuffer::Static);
        }
        const std::array<GLVertex2D, 4> data{
            GLVertex2D{
                .position = QVector2D(0, 0),
                .texcoord = QVector2D(p1),
            },
            GLVertex2D{
                .position = QVector2D(0, destinationSize.height()),
                .texcoord = QVector2D(p2),
            },
            GLVertex2D{
                .position = QVector2D(destinationSize.width(), 0),
                .texcoord = QVector2D(p3),
            },
            GLVertex2D{
                .position = QVector2D(destinationSize.width(), destinationSize.height()),
                .texcoord = QVector2D(p4),
            },
        };
        d->m_vbo->setVertices(data);
    }
    bind();
    d->m_vbo->render(region, GL_TRIANGLE_STRIP, hardwareClipping);
    unbind();
}

GLuint GLTexture::texture() const
{
    return d->m_texture;
}

GLenum GLTexture::target() const
{
    return d->m_target;
}

GLenum GLTexture::filter() const
{
    return d->m_filter;
}

GLenum GLTexture::internalFormat() const
{
    return d->m_internalFormat;
}

bool GLTexture::isDirty() const
{
    return d->m_markedDirty;
}

void GLTexture::setFilter(GLenum filter)
{
    if (filter != d->m_filter) {
        d->m_filter = filter;
        d->m_filterChanged = true;
    }
}

void GLTexture::setWrapMode(GLenum mode)
{
    if (mode != d->m_wrapMode) {
        d->m_wrapMode = mode;
        d->m_wrapModeChanged = true;
    }
}

void GLTexture::setDirty()
{
    d->m_markedDirty = true;
}

void GLTexture::onDamage()
{
}

void GLTexturePrivate::updateMatrix()
{
    const QMatrix4x4 textureToBufferMatrix = m_textureToBufferTransform.toMatrix();

    m_matrix[NormalizedCoordinates].setToIdentity();
    m_matrix[UnnormalizedCoordinates].setToIdentity();

    if (m_target == GL_TEXTURE_RECTANGLE_ARB) {
        m_matrix[NormalizedCoordinates].scale(m_size.width(), m_size.height());
    } else {
        m_matrix[UnnormalizedCoordinates].scale(1.0 / m_size.width(), 1.0 / m_size.height());
    }

    m_matrix[NormalizedCoordinates].translate(0.5, 0.5);
    // our Y axis is flipped vs OpenGL
    m_matrix[NormalizedCoordinates].scale(1, -1);
    m_matrix[NormalizedCoordinates] *= textureToBufferMatrix;
    m_matrix[NormalizedCoordinates].translate(-0.5, -0.5);

    m_matrix[UnnormalizedCoordinates].translate(m_size.width() / 2, m_size.height() / 2);
    m_matrix[UnnormalizedCoordinates].scale(1, -1);
    m_matrix[UnnormalizedCoordinates] *= textureToBufferMatrix;
    m_matrix[UnnormalizedCoordinates].translate(-m_size.width() / 2, -m_size.height() / 2);
}

void GLTexture::setContentTransform(OutputTransform transform)
{
    if (d->m_textureToBufferTransform != transform) {
        d->m_textureToBufferTransform = transform;
        d->updateMatrix();
    }
}

OutputTransform GLTexture::contentTransform() const
{
    return d->m_textureToBufferTransform;
}

void GLTexture::setSwizzle(GLenum red, GLenum green, GLenum blue, GLenum alpha)
{
    if (!OpenGlContext::currentContext()->isOpenGLES()) {
        const GLuint swizzle[] = {red, green, blue, alpha};
        glTexParameteriv(d->m_target, GL_TEXTURE_SWIZZLE_RGBA, (const GLint *)swizzle);
    } else {
        glTexParameteri(d->m_target, GL_TEXTURE_SWIZZLE_R, red);
        glTexParameteri(d->m_target, GL_TEXTURE_SWIZZLE_G, green);
        glTexParameteri(d->m_target, GL_TEXTURE_SWIZZLE_B, blue);
        glTexParameteri(d->m_target, GL_TEXTURE_SWIZZLE_A, alpha);
    }
}

int GLTexture::width() const
{
    return d->m_size.width();
}

int GLTexture::height() const
{
    return d->m_size.height();
}

QMatrix4x4 GLTexture::matrix(TextureCoordinateType type) const
{
    return d->m_matrix[type];
}

bool GLTexture::supportsSwizzle()
{
    return OpenGlContext::currentContext()->supportsTextureSwizzle();
}

bool GLTexture::supportsFormatRG()
{
    return OpenGlContext::currentContext()->supportsRGTextures();
}

QImage GLTexture::toImage()
{
    if (target() != GL_TEXTURE_2D) {
        return QImage();
    }
    QImage ret(size(), QImage::Format_RGBA8888_Premultiplied);

    if (OpenGlContext::currentContext()->isOpenGLES()) {
        GLFramebuffer fbo(this);
        GLFramebuffer::pushFramebuffer(&fbo);
        glReadPixels(0, 0, width(), height(), GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, ret.bits());
        GLFramebuffer::popFramebuffer();
    } else {
        GLint currentTextureBinding;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &currentTextureBinding);
        if (GLuint(currentTextureBinding) != texture()) {
            glBindTexture(GL_TEXTURE_2D, texture());
        }
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, ret.bits());
        if (GLuint(currentTextureBinding) != texture()) {
            glBindTexture(GL_TEXTURE_2D, currentTextureBinding);
        }
    }
    return ret;
}

std::unique_ptr<GLTexture> GLTexture::createNonOwningWrapper(GLuint textureId, GLenum internalFormat, const QSize &size)
{
    return std::unique_ptr<GLTexture>(new GLTexture(GL_TEXTURE_2D, textureId, internalFormat, size, 1, false, OutputTransform{}));
}

std::unique_ptr<GLTexture> GLTexture::allocate(GLenum internalFormat, const QSize &size, int levels)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) {
        qCWarning(KWIN_OPENGL, "generating OpenGL texture handle failed");
        return nullptr;
    }
    glBindTexture(GL_TEXTURE_2D, texture);

    const auto context = OpenGlContext::currentContext();
    if (!context->isOpenGLES()) {
        if (context->supportsTextureStorage()) {
            glTexStorage2D(GL_TEXTURE_2D, levels, internalFormat, size.width(), size.height());
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels - 1);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, size.width(), size.height(), 0,
                         GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, nullptr);
        }
    } else {
        // The format parameter in glTexSubImage() must match the internal format
        // of the texture, so it's important that we allocate the texture with
        // the format that will be used in update() and clear().
        const GLenum format = context->supportsARGB32Textures() ? GL_BGRA_EXT : GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, format, size.width(), size.height(), 0,
                     format, GL_UNSIGNED_BYTE, nullptr);

        // The internalFormat is technically not correct, but it means that code that calls
        // internalFormat() won't need to be specialized for GLES2.
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return std::unique_ptr<GLTexture>(new GLTexture(GL_TEXTURE_2D, texture, internalFormat, size, levels, true, OutputTransform{}));
}

std::unique_ptr<GLTexture> GLTexture::upload(const QImage &image)
{
    if (image.isNull()) {
        return nullptr;
    }
    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) {
        qCWarning(KWIN_OPENGL, "generating OpenGL texture handle failed");
        return nullptr;
    }

    const auto context = OpenGlContext::currentContext();
    GLenum internalFormat;
    GLenum format;
    GLenum type;
    QImage::Format uploadFormat;
    if (!context->isOpenGLES()) {
        const QImage::Format index = image.format();
        if (index < sizeof(formatTable) / sizeof(formatTable[0]) && formatTable[index].internalFormat) {
            internalFormat = formatTable[index].internalFormat;
            format = formatTable[index].format;
            type = formatTable[index].type;
            uploadFormat = index;
        } else {
            internalFormat = GL_RGBA8;
            format = GL_BGRA;
            type = GL_UNSIGNED_INT_8_8_8_8_REV;
            uploadFormat = QImage::Format_ARGB32_Premultiplied;
        }
    } else {
        if (context->supportsARGB32Textures()) {
            internalFormat = GL_BGRA_EXT;
            format = GL_BGRA_EXT;
            type = GL_UNSIGNED_BYTE;
            uploadFormat = QImage::Format_ARGB32_Premultiplied;
        } else {
            internalFormat = GL_RGBA;
            format = GL_RGBA;
            type = GL_UNSIGNED_BYTE;
            uploadFormat = QImage::Format_RGBA8888_Premultiplied;
        }
    }

    QImage im = image;
    if (im.format() != uploadFormat) {
        im.convertTo(uploadFormat);
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, im.bytesPerLine() / (im.depth() / 8));
    if (!context->isOpenGLES()) {
        if (context->supportsTextureStorage()) {
            glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, im.width(), im.height());
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, im.width(), im.height(), format, type, im.constBits());
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, im.width(), im.height(), 0, format, type, im.constBits());
        }
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, im.width(), im.height(), 0, format, type, im.constBits());
    }
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    return std::unique_ptr<GLTexture>(new GLTexture(GL_TEXTURE_2D, texture, internalFormat, image.size(), 1, true, OutputTransform::FlipY));
}

std::unique_ptr<GLTexture> GLTexture::upload(const QPixmap &pixmap)
{
    return upload(pixmap.toImage());
}

} // namespace KWin
