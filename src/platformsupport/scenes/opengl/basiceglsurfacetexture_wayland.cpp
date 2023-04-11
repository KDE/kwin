/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platformsupport/scenes/opengl/basiceglsurfacetexture_wayland.h"
#include "libkwineffects/kwingltexture.h"
#include "platformsupport/scenes/opengl/abstract_egl_backend.h"
#include "scene/surfaceitem_wayland.h"
#include "utils/common.h"
#include "wayland/linuxdmabufv1clientbuffer.h"
#include "wayland/shmclientbuffer.h"

#include <epoxy/egl.h>

namespace KWin
{

BasicEGLSurfaceTextureWayland::BasicEGLSurfaceTextureWayland(OpenGLBackend *backend,
                                                             SurfacePixmapWayland *pixmap)
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
    if (auto buffer = qobject_cast<KWaylandServer::LinuxDmaBufV1ClientBuffer *>(m_pixmap->buffer())) {
        return loadDmabufTexture(buffer);
    } else if (auto buffer = qobject_cast<KWaylandServer::ShmClientBuffer *>(m_pixmap->buffer())) {
        return loadShmTexture(buffer);
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
    if (auto buffer = qobject_cast<KWaylandServer::LinuxDmaBufV1ClientBuffer *>(m_pixmap->buffer())) {
        updateDmabufTexture(buffer);
    } else if (auto buffer = qobject_cast<KWaylandServer::ShmClientBuffer *>(m_pixmap->buffer())) {
        updateShmTexture(buffer, region);
    }
}

bool BasicEGLSurfaceTextureWayland::loadShmTexture(KWaylandServer::ShmClientBuffer *buffer)
{
    const QImage &image = buffer->data();
    if (Q_UNLIKELY(image.isNull())) {
        return false;
    }

    m_texture.reset(new GLTexture(image));
    m_texture->setFilter(GL_LINEAR);
    m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
    m_texture->setContentTransform(TextureTransform::MirrorY);
    m_bufferType = BufferType::Shm;

    return true;
}

void BasicEGLSurfaceTextureWayland::updateShmTexture(KWaylandServer::ShmClientBuffer *buffer, const QRegion &region)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::Shm)) {
        destroy();
        create();
        return;
    }

    const QImage &image = buffer->data();
    if (Q_UNLIKELY(image.isNull())) {
        return;
    }

    const QRegion damage = mapRegion(m_pixmap->item()->surfaceToBufferMatrix(), region);
    for (const QRect &rect : damage) {
        m_texture->update(image, rect.topLeft(), rect);
    }
}

bool BasicEGLSurfaceTextureWayland::loadDmabufTexture(KWaylandServer::LinuxDmaBufV1ClientBuffer *buffer)
{
    EGLImageKHR image = backend()->importBufferAsImage(buffer);
    if (Q_UNLIKELY(image == EGL_NO_IMAGE_KHR)) {
        qCritical(KWIN_OPENGL) << "Invalid dmabuf-based wl_buffer";
        return false;
    }

    m_texture.reset(new GLTexture(GL_TEXTURE_2D));
    m_texture->setSize(buffer->size());
    m_texture->create();
    m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
    m_texture->setFilter(GL_NEAREST);
    m_texture->bind();
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(image));
    m_texture->unbind();
    m_texture->setContentTransform(buffer->origin() == KWaylandServer::ClientBuffer::Origin::TopLeft ? TextureTransform::MirrorY : TextureTransforms());
    m_bufferType = BufferType::DmaBuf;

    return true;
}

void BasicEGLSurfaceTextureWayland::updateDmabufTexture(KWaylandServer::LinuxDmaBufV1ClientBuffer *buffer)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::DmaBuf)) {
        destroy();
        create();
        return;
    }

    m_texture->bind();
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(backend()->importBufferAsImage(buffer)));
    m_texture->unbind();
    // The origin in a dmabuf-buffer is at the upper-left corner, so the meaning
    // of Y-inverted is the inverse of OpenGL.
    m_texture->setContentTransform(buffer->origin() == KWaylandServer::ClientBuffer::Origin::TopLeft ? TextureTransform::MirrorY : TextureTransforms());
}

} // namespace KWin
