/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platformsupport/scenes/opengl/openglsurfacetexture.h"
#include "core/graphicsbufferview.h"
#include "opengl/gltexture.h"
#include "platformsupport/scenes/opengl/abstract_egl_backend.h"
#include "utils/common.h"

#include <epoxy/egl.h>
#include <utils/drm_format_helper.h>

namespace KWin
{

OpenGLSurfaceTexture::OpenGLSurfaceTexture(AbstractEglBackend *backend, SurfacePixmap *pixmap)
    : m_backend(backend)
    , m_pixmap(pixmap)
{
}

OpenGLSurfaceTexture::~OpenGLSurfaceTexture()
{
    destroy();
}

bool OpenGLSurfaceTexture::isValid() const
{
    return m_texture.isValid();
}

OpenGLSurfaceContents OpenGLSurfaceTexture::texture() const
{
    return m_texture;
}

bool OpenGLSurfaceTexture::create()
{
    GraphicsBuffer *buffer = m_pixmap->item()->buffer();
    if (buffer->dmabufAttributes()) {
        return loadDmabufTexture(buffer);
    } else if (buffer->shmAttributes()) {
        return loadShmTexture(buffer);
    } else {
        qCDebug(KWIN_OPENGL) << "Failed to create OpenGLSurfaceTexture for a buffer of unknown type" << buffer;
        return false;
    }
}

void OpenGLSurfaceTexture::destroy()
{
    m_texture.reset();
    m_bufferType = BufferType::None;
}

void OpenGLSurfaceTexture::update(const QRegion &region)
{
    GraphicsBuffer *buffer = m_pixmap->item()->buffer();
    if (buffer->dmabufAttributes()) {
        updateDmabufTexture(buffer);
    } else if (buffer->shmAttributes()) {
        updateShmTexture(buffer, region);
    } else {
        qCDebug(KWIN_OPENGL) << "Failed to update OpenGLSurfaceTexture for a buffer of unknown type" << buffer;
    }
}

bool OpenGLSurfaceTexture::loadShmTexture(GraphicsBuffer *buffer)
{
    const GraphicsBufferView view(buffer);
    if (Q_UNLIKELY(view.isNull())) {
        return false;
    }

    std::shared_ptr<GLTexture> texture = GLTexture::upload(*view.image());
    if (Q_UNLIKELY(!texture)) {
        return false;
    }

    texture->setFilter(GL_LINEAR);
    texture->setWrapMode(GL_CLAMP_TO_EDGE);
    texture->setContentTransform(OutputTransform::FlipY);

    m_texture = {{texture}};

    m_bufferType = BufferType::Shm;

    return true;
}

static QRegion simplifyDamage(const QRegion &damage)
{
    if (damage.rectCount() < 3) {
        return damage;
    } else {
        return damage.boundingRect();
    }
}

void OpenGLSurfaceTexture::updateShmTexture(GraphicsBuffer *buffer, const QRegion &region)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::Shm)) {
        destroy();
        create();
        return;
    }

    const GraphicsBufferView view(buffer);
    if (Q_UNLIKELY(view.isNull())) {
        return;
    }

    m_texture.planes[0]->update(*view.image(), simplifyDamage(region));
}

bool OpenGLSurfaceTexture::loadDmabufTexture(GraphicsBuffer *buffer)
{
    auto createTexture = [this](EGLImageKHR image, const QSize &size, bool isExternalOnly) -> std::shared_ptr<GLTexture> {
        if (Q_UNLIKELY(image == EGL_NO_IMAGE_KHR)) {
            qCritical(KWIN_OPENGL) << "Invalid dmabuf-based wl_buffer";
            return nullptr;
        }

        GLint target = isExternalOnly ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
        auto texture = std::make_shared<GLTexture>(target);
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
        QList<std::shared_ptr<GLTexture>> textures;
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
            textures << t;
        }
        m_texture = {textures};
    } else {
        const bool isExternal = m_backend->eglDisplayObject()->isExternalOnly(attribs->format, attribs->modifier);
        auto texture = createTexture(m_backend->importBufferAsImage(buffer), buffer->size(), isExternal);
        if (!texture) {
            return false;
        }
        m_texture = {{texture}};
    }
    m_bufferType = BufferType::DmaBuf;

    return true;
}

void OpenGLSurfaceTexture::updateDmabufTexture(GraphicsBuffer *buffer)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::DmaBuf)) {
        destroy();
        create();
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

            m_texture.planes[plane]->bind();
            glEGLImageTargetTexture2DOES(target, static_cast<GLeglImageOES>(m_backend->importBufferAsImage(buffer, plane, currentPlane.format, size)));
            m_texture.planes[plane]->unbind();
        }
    } else {
        Q_ASSERT(m_texture.planes.count() == 1);
        m_texture.planes[0]->bind();
        glEGLImageTargetTexture2DOES(target, static_cast<GLeglImageOES>(m_backend->importBufferAsImage(buffer)));
        m_texture.planes[0]->unbind();
    }
}

} // namespace KWin
