/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwingltextureuploader.h"
#include "kwinglplatform.h"
#include "kwingltexture.h"
#include "kwingltexture_p.h"

namespace KWin
{

class GLTextureUploaderPrivate
{
public:
    GLuint pbo = 0;
    GLsizeiptr size = 0;
};

GLTextureUploader::GLTextureUploader()
    : d(new GLTextureUploaderPrivate)
{
}

GLTextureUploader::~GLTextureUploader()
{
    if (d->pbo) {
        glDeleteBuffers(1, &d->pbo);
    }
}

void GLTextureUploader::upload(GLTexture *texture, const QImage &source, const QRegion &region)
{
    GLenum glFormat;
    GLenum type;
    QImage::Format uploadFormat;

    if (!GLPlatform::instance()->isGLES()) {
        const QImage::Format index = source.format();
        if (index < sizeof(GLTexturePrivate::s_formatTable) / sizeof(GLTexturePrivate::s_formatTable[0]) && GLTexturePrivate::s_formatTable[index].internalFormat
            && !(GLTexturePrivate::s_formatTable[index].type == GL_UNSIGNED_SHORT && !GLTexturePrivate::s_supportsTexture16Bit)) {
            glFormat = GLTexturePrivate::s_formatTable[index].format;
            type = GLTexturePrivate::s_formatTable[index].type;
            uploadFormat = index;
        } else {
            glFormat = GL_BGRA;
            type = GL_UNSIGNED_INT_8_8_8_8_REV;
            uploadFormat = QImage::Format_ARGB32_Premultiplied;
        }
    } else {
        if (GLTexturePrivate::s_supportsARGB32) {
            glFormat = GL_BGRA_EXT;
            type = GL_UNSIGNED_BYTE;
            uploadFormat = QImage::Format_ARGB32_Premultiplied;
        } else {
            glFormat = GL_RGBA;
            type = GL_UNSIGNED_BYTE;
            uploadFormat = QImage::Format_RGBA8888_Premultiplied;
        }
    }

    static const bool unpackBufferDisabled = qEnvironmentVariableIntValue("KWIN_GL_NO_UNPACK_BUFFER") == 1;
    if (!unpackBufferDisabled) {
        if (!d->pbo) {
            glGenBuffers(1, &d->pbo);
        }

        QImage data = source;
        if (data.format() != uploadFormat) {
            data.convertTo(uploadFormat);
        }

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, d->pbo);
        if (d->size != data.sizeInBytes()) {
            d->size = data.sizeInBytes();
            glBufferData(GL_PIXEL_UNPACK_BUFFER, data.sizeInBytes(), nullptr, GL_STREAM_DRAW);
        }

        GLubyte *ptr = static_cast<GLubyte *>(glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
        if (ptr) {
            std::copy_n(data.constBits(), data.sizeInBytes(), ptr);
            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        }

        glBindTexture(GL_TEXTURE_2D, texture->texture());
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, data.width(), data.height(), glFormat, type, nullptr);

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    } else {
        const bool useUnpack = GLTexturePrivate::s_supportsUnpack && source.format() == uploadFormat;

        glBindTexture(GL_TEXTURE_2D, texture->texture());

        for (const QRect &rect : region) {
            QImage data = source;
            if (useUnpack) {
                Q_ASSERT(data.depth() % 8 == 0);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, data.bytesPerLine() / (data.depth() / 8));
                glPixelStorei(GL_UNPACK_SKIP_PIXELS, rect.x());
                glPixelStorei(GL_UNPACK_SKIP_ROWS, rect.y());
            } else {
                data = source.copy(rect);
                if (data.format() != uploadFormat) {
                    data.convertTo(uploadFormat);
                }
            }

            glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x(), rect.y(), rect.width(), rect.height(), glFormat, type, data.constBits());
        }

        if (useUnpack) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        }

        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

} // namespace KWin
