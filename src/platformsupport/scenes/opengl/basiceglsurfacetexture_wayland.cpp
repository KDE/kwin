/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platformsupport/scenes/opengl/basiceglsurfacetexture_wayland.h"
#include "core/graphicsbufferview.h"
#include "opengl/gltexture.h"
#include "platformsupport/scenes/opengl/abstract_egl_backend.h"
#include "utils/common.h"

#include <epoxy/egl.h>

namespace KWin
{

BasicEGLSurfaceTextureWayland::BasicEGLSurfaceTextureWayland(OpenGLBackend *backend, SurfacePixmap *pixmap)
    : OpenGLSurfaceTextureWayland(backend, pixmap)
{
}

BasicEGLSurfaceTextureWayland::~BasicEGLSurfaceTextureWayland()
{
    destroy();
}

AbstractEglBackend *BasicEGLSurfaceTextureWayland::backend() const
{
    return static_cast<AbstractEglBackend *>(m_backend);
}

bool BasicEGLSurfaceTextureWayland::create()
{
    if (m_pixmap->buffer()->dmabufAttributes()) {
        return loadDmabufTexture(m_pixmap->buffer());
    } else if (m_pixmap->buffer()->shmAttributes()) {
        return loadShmTexture(m_pixmap->buffer());
    } else {
        return false;
    }
}

void BasicEGLSurfaceTextureWayland::destroy()
{
    m_texture.reset();
    m_bufferType = BufferType::None;
}

void BasicEGLSurfaceTextureWayland::update(const QRegion &region)
{
    if (m_pixmap->buffer()->dmabufAttributes()) {
        updateDmabufTexture(m_pixmap->buffer());
    } else if (m_pixmap->buffer()->shmAttributes()) {
        updateShmTexture(m_pixmap->buffer(), region);
    }
}

bool BasicEGLSurfaceTextureWayland::loadShmTexture(GraphicsBuffer *buffer)
{
    const GraphicsBufferView view(buffer);
    if (Q_UNLIKELY(!view.image())) {
        return false;
    }

    m_texture = GLTexture::upload(*view.image());
    if (Q_UNLIKELY(!m_texture)) {
        return false;
    }

    m_texture->setFilter(GL_LINEAR);
    m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
    m_texture->setContentTransform(TextureTransform::MirrorY);
    m_bufferType = BufferType::Shm;

    return true;
}

void BasicEGLSurfaceTextureWayland::updateShmTexture(GraphicsBuffer *buffer, const QRegion &region)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::Shm)) {
        destroy();
        create();
        return;
    }

    const GraphicsBufferView view(buffer);
    if (Q_UNLIKELY(!view.image())) {
        return;
    }

    for (const QRect &rect : region) {
        m_texture->update(*view.image(), rect.topLeft(), rect);
    }
}

bool BasicEGLSurfaceTextureWayland::loadDmabufTexture(GraphicsBuffer *buffer)
{
    EGLImageKHR image = backend()->importBufferAsImage(buffer);
    if (Q_UNLIKELY(image == EGL_NO_IMAGE_KHR)) {
        qCritical(KWIN_OPENGL) << "Invalid dmabuf-based wl_buffer";
        return false;
    }

    m_texture = std::make_unique<GLTexture>(GL_TEXTURE_2D);
    m_texture->setSize(buffer->size());
    m_texture->create();
    m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
    m_texture->setFilter(GL_LINEAR);
    m_texture->bind();
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(image));
    m_texture->unbind();
    if (m_pixmap->bufferOrigin() == GraphicsBufferOrigin::TopLeft) {
        m_texture->setContentTransform(TextureTransform::MirrorY);
    }
    m_bufferType = BufferType::DmaBuf;

    return true;
}

void BasicEGLSurfaceTextureWayland::updateDmabufTexture(GraphicsBuffer *buffer)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::DmaBuf)) {
        destroy();
        create();
        return;
    }

    m_texture->bind();
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(backend()->importBufferAsImage(buffer)));
    m_texture->unbind();
}

} // namespace KWin
