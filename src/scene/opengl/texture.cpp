/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/opengl/texture.h"
#include "compositor.h"
#include "core/graphicsbufferview.h"
#include "opengl/eglbackend.h"
#include "opengl/gltexture.h"
#include "utils/common.h"
#include "utils/drm_format_helper.h"

namespace KWin
{

TextureOpenGL::~TextureOpenGL()
{
    qDeleteAll(m_planes);
}

QVarLengthArray<GLTexture *, 4> TextureOpenGL::planes() const
{
    return m_planes;
}

std::unique_ptr<ImageTextureOpenGL> ImageTextureOpenGL::create(const QImage &image)
{
    auto texture = std::make_unique<ImageTextureOpenGL>();
    if (texture->upload(image)) {
        return texture;
    }

    return nullptr;
}

void ImageTextureOpenGL::attach(GraphicsBuffer *buffer, const Region &region)
{
    Q_UNREACHABLE();
}

bool ImageTextureOpenGL::upload(const QImage &image)
{
    auto nativeTexture = GLTexture::upload(image);
    if (!nativeTexture) {
        return false;
    }

    nativeTexture->setFilter(GL_LINEAR);
    nativeTexture->setWrapMode(GL_CLAMP_TO_EDGE);

    m_planes = {nativeTexture.release()};
    m_size = m_planes[0]->size();

    return true;
}

void ImageTextureOpenGL::upload(const QImage &image, const Rect &rect)
{
    m_planes[0]->update(image, rect);
}

BufferTextureOpenGL::BufferTextureOpenGL(EglBackend *backend)
    : m_backend(backend)
{
}

std::unique_ptr<BufferTextureOpenGL> BufferTextureOpenGL::create(GraphicsBuffer *buffer)
{
    auto texture = std::make_unique<BufferTextureOpenGL>(static_cast<EglBackend *>(Compositor::self()->backend()));
    if (texture->attach(buffer)) {
        return texture;
    }

    return nullptr;
}

bool BufferTextureOpenGL::attach(GraphicsBuffer *buffer)
{
    if (buffer->dmabufAttributes()) {
        return loadDmabufTexture(buffer);
    } else if (buffer->shmAttributes()) {
        return loadShmTexture(buffer);
    } else if (buffer->singlePixelAttributes()) {
        return loadSinglePixelTexture(buffer);
    } else {
        qCDebug(KWIN_OPENGL) << "Failed to create OpenGLSurfaceTexture for a buffer of unknown type" << buffer;
        return false;
    }
}

void BufferTextureOpenGL::attach(GraphicsBuffer *buffer, const Region &region)
{
    if (buffer->dmabufAttributes()) {
        updateDmabufTexture(buffer);
    } else if (buffer->shmAttributes()) {
        updateShmTexture(buffer, region);
    } else if (buffer->singlePixelAttributes()) {
        updateSinglePixelTexture(buffer);
    } else {
        qCDebug(KWIN_OPENGL) << "Failed to update OpenGLSurfaceTexture for a buffer of unknown type" << buffer;
    }
}

void BufferTextureOpenGL::upload(const QImage &image, const Rect &rect)
{
    Q_UNREACHABLE();
}

void BufferTextureOpenGL::reset()
{
    qDeleteAll(m_planes);
    m_planes.clear();
    m_bufferType = BufferType::None;
    m_size = QSize();
}

bool BufferTextureOpenGL::loadShmTexture(GraphicsBuffer *buffer)
{
    const GraphicsBufferView view(buffer);
    if (Q_UNLIKELY(view.isNull())) {
        return false;
    }

    std::unique_ptr<GLTexture> texture = GLTexture::upload(*view.image());
    if (Q_UNLIKELY(!texture)) {
        return false;
    }

    texture->setFilter(GL_LINEAR);
    texture->setWrapMode(GL_CLAMP_TO_EDGE);
    texture->setContentTransform(OutputTransform::FlipY);

    m_bufferType = BufferType::Shm;
    m_planes = {texture.release()};
    m_size = buffer->size();
    const auto info = FormatInfo::get(buffer->shmAttributes()->format);
    m_isFloatingPoint = info && info->floatingPoint;

    return true;
}

static Region simplifyDamage(const Region &damage)
{
    if (damage.rects().size() < 3) {
        return damage;
    } else {
        return damage.boundingRect();
    }
}

void BufferTextureOpenGL::updateShmTexture(GraphicsBuffer *buffer, const Region &region)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::Shm)) {
        reset();
        attach(buffer);
        return;
    }

    const GraphicsBufferView view(buffer);
    if (Q_UNLIKELY(view.isNull())) {
        return;
    }

    m_planes[0]->update(*view.image(), simplifyDamage(region));
    const auto info = FormatInfo::get(buffer->shmAttributes()->format);
    m_isFloatingPoint = info && info->floatingPoint;
}

bool BufferTextureOpenGL::loadDmabufTexture(GraphicsBuffer *buffer)
{
    auto createTexture = [](EGLImageKHR image, const QSize &size, bool isExternalOnly) -> std::unique_ptr<GLTexture> {
        if (Q_UNLIKELY(image == EGL_NO_IMAGE_KHR)) {
            qCritical(KWIN_OPENGL) << "Invalid dmabuf-based wl_buffer";
            return nullptr;
        }

        GLint target = isExternalOnly ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
        auto texture = std::make_unique<GLTexture>(target);
        texture->setSize(size);
        if (!texture->create()) {
            return nullptr;
        }
        texture->setWrapMode(GL_CLAMP_TO_EDGE);
        texture->setFilter(GL_LINEAR);
        texture->bind();
        glEGLImageTargetTexture2DOES(target, static_cast<GLeglImageOES>(image));
        texture->unbind();
        texture->setContentTransform(OutputTransform::FlipY);
        return texture;
    };

    const auto attribs = buffer->dmabufAttributes();
    if (auto itConv = s_drmConversions.find(buffer->dmabufAttributes()->format); itConv != s_drmConversions.end()) {
        std::vector<std::unique_ptr<GLTexture>> textures;
        Q_ASSERT(itConv->plane.count() == uint(buffer->dmabufAttributes()->planeCount));

        for (uint plane = 0; plane < itConv->plane.count(); ++plane) {
            const auto &currentPlane = itConv->plane[plane];
            QSize size = buffer->size();
            size.rwidth() /= currentPlane.widthDivisor;
            size.rheight() /= currentPlane.heightDivisor;

            const bool isExternal = m_backend->eglDisplayObject()->isExternalOnly(currentPlane.format, attribs->modifier);
            auto t = createTexture(m_backend->importBufferAsImage(buffer, plane, currentPlane.format, size), size, isExternal);
            if (!t) {
                return false;
            }
            textures.emplace_back(std::move(t));
        }
        for (auto &texture : textures) {
            m_planes.append(texture.release());
        }
    } else {
        const bool isExternal = m_backend->eglDisplayObject()->isExternalOnly(attribs->format, attribs->modifier);
        auto texture = createTexture(m_backend->importBufferAsImage(buffer), buffer->size(), isExternal);
        if (!texture) {
            return false;
        }
        m_planes = {texture.release()};
    }

    m_bufferType = BufferType::DmaBuf;
    m_size = buffer->size();
    const auto info = FormatInfo::get(buffer->dmabufAttributes()->format);
    m_isFloatingPoint = info && info->floatingPoint;

    return true;
}

void BufferTextureOpenGL::updateDmabufTexture(GraphicsBuffer *buffer)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::DmaBuf)) {
        reset();
        attach(buffer);
        return;
    }

    const GLint target = GL_TEXTURE_2D;
    if (auto itConv = s_drmConversions.find(buffer->dmabufAttributes()->format); itConv != s_drmConversions.end()) {
        Q_ASSERT(itConv->plane.count() == uint(buffer->dmabufAttributes()->planeCount));
        for (uint plane = 0; plane < itConv->plane.count(); ++plane) {
            const auto &currentPlane = itConv->plane[plane];
            QSize size = buffer->size();
            size.rwidth() /= currentPlane.widthDivisor;
            size.rheight() /= currentPlane.heightDivisor;

            m_planes[plane]->bind();
            glEGLImageTargetTexture2DOES(target, static_cast<GLeglImageOES>(m_backend->importBufferAsImage(buffer, plane, currentPlane.format, size)));
            m_planes[plane]->unbind();
        }
    } else {
        Q_ASSERT(m_planes.count() == 1);
        m_planes[0]->bind();
        glEGLImageTargetTexture2DOES(target, static_cast<GLeglImageOES>(m_backend->importBufferAsImage(buffer)));
        m_planes[0]->unbind();
    }
    const auto info = FormatInfo::get(buffer->dmabufAttributes()->format);
    m_isFloatingPoint = info && info->floatingPoint;
}

bool BufferTextureOpenGL::loadSinglePixelTexture(GraphicsBuffer *buffer)
{
    // TODO this shouldn't allocate a texture,
    // the renderer should just use a color in the shader
    const GraphicsBufferView view(buffer);
    std::unique_ptr<GLTexture> texture = GLTexture::upload(*view.image());
    if (Q_UNLIKELY(!texture)) {
        return false;
    }
    m_planes = {texture.release()};
    m_bufferType = BufferType::SinglePixel;
    m_size = QSize(1, 1);
    m_isFloatingPoint = false;
    return true;
}

void BufferTextureOpenGL::updateSinglePixelTexture(GraphicsBuffer *buffer)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::SinglePixel)) {
        reset();
        attach(buffer);
        return;
    }
    const GraphicsBufferView view(buffer);
    m_planes[0]->update(*view.image(), Rect(0, 0, 1, 1));
}

} // namespace KWin
