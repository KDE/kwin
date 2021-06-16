/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "basiceglsurfacetextureprovider_wayland.h"
#include "egl_dmabuf.h"
#include "kwineglext.h"
#include "kwingltexture.h"
#include "logging.h"
#include "surfaceitem_wayland.h"

#include <KWaylandServer/buffer_interface.h>
#include <KWaylandServer/linuxdmabuf_v1_interface.h>
#include <KWaylandServer/surface_interface.h>

namespace KWin
{

BasicEGLSurfaceTextureProviderWayland::BasicEGLSurfaceTextureProviderWayland(OpenGLBackend *backend,
                                                                             SurfacePixmapWayland *pixmap)
    : OpenGLSurfaceTextureProviderWayland(backend, pixmap)
{
}

BasicEGLSurfaceTextureProviderWayland::~BasicEGLSurfaceTextureProviderWayland()
{
    destroy();
}

AbstractEglBackend *BasicEGLSurfaceTextureProviderWayland::backend() const
{
    return static_cast<AbstractEglBackend *>(m_backend);
}

bool BasicEGLSurfaceTextureProviderWayland::create()
{
    KWaylandServer::BufferInterface *buffer = m_pixmap->buffer();
    if (Q_UNLIKELY(!buffer)) {
        return false;
    }
    if (buffer->linuxDmabufBuffer()) {
        return loadDmabufTexture(buffer);
    } else if (buffer->shmBuffer()) {
        return loadShmTexture(buffer);
    } else {
        return loadEglTexture(buffer);
    }
}

void BasicEGLSurfaceTextureProviderWayland::destroy()
{
    if (m_image != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(backend()->eglDisplay(), m_image);
        m_image = EGL_NO_IMAGE_KHR;
    }
    m_texture.reset();
    m_bufferType = BufferType::None;
}

void BasicEGLSurfaceTextureProviderWayland::update(const QRegion &region)
{
    KWaylandServer::BufferInterface *buffer = m_pixmap->buffer();
    if (Q_UNLIKELY(!buffer)) {
        return;
    }
    if (buffer->linuxDmabufBuffer()) {
        updateDmabufTexture(buffer);
    } else if (buffer->shmBuffer()) {
        updateShmTexture(buffer, region);
    } else {
        updateEglTexture(buffer);
    }
}

bool BasicEGLSurfaceTextureProviderWayland::loadShmTexture(KWaylandServer::BufferInterface *buffer)
{
    const QImage &image = buffer->data();
    if (Q_UNLIKELY(image.isNull())) {
        return false;
    }

    m_texture.reset(new GLTexture(image));
    m_texture->setFilter(GL_LINEAR);
    m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
    m_texture->setYInverted(true);
    m_bufferType = BufferType::Shm;

    return true;
}

void BasicEGLSurfaceTextureProviderWayland::updateShmTexture(KWaylandServer::BufferInterface *buffer, const QRegion &region)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::Shm)) {
        destroy();
        create();
        return;
    }

    KWaylandServer::SurfaceInterface *surface = m_pixmap->surface();
    if (Q_UNLIKELY(!surface)) {
        return;
    }

    const QImage &image = buffer->data();
    if (Q_UNLIKELY(image.isNull())) {
        return;
    }

    const QRegion damage = surface->mapToBuffer(region);
    for (const QRect &rect : damage) {
        m_texture->update(image, rect.topLeft(), rect);
    }
}

bool BasicEGLSurfaceTextureProviderWayland::loadEglTexture(KWaylandServer::BufferInterface *buffer)
{
    const AbstractEglBackendFunctions *funcs = backend()->functions();
    if (Q_UNLIKELY(!funcs->eglQueryWaylandBufferWL)) {
        return false;
    }
    if (Q_UNLIKELY(!buffer->resource())) {
        return false;
    }

    m_texture.reset(new GLTexture(GL_TEXTURE_2D));
    m_texture->setSize(buffer->size());
    m_texture->create();
    m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
    m_texture->setFilter(GL_LINEAR);
    m_texture->bind();
    m_image = attach(buffer);
    m_texture->unbind();
    m_bufferType = BufferType::Egl;

    if (EGL_NO_IMAGE_KHR == m_image) {
        qCDebug(KWIN_OPENGL) << "failed to create egl image";
        m_texture.reset();
        return false;
    }

    return true;
}

void BasicEGLSurfaceTextureProviderWayland::updateEglTexture(KWaylandServer::BufferInterface *buffer)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::Egl)) {
        destroy();
        create();
        return;
    }

    m_texture->bind();
    EGLImageKHR image = attach(buffer);
    m_texture->unbind();
    if (image != EGL_NO_IMAGE_KHR) {
        if (m_image != EGL_NO_IMAGE_KHR) {
            eglDestroyImageKHR(backend()->eglDisplay(), m_image);
        }
        m_image = image;
    }
}

bool BasicEGLSurfaceTextureProviderWayland::loadDmabufTexture(KWaylandServer::BufferInterface *buffer)
{
    auto dmabuf = static_cast<EglDmabufBuffer *>(buffer->linuxDmabufBuffer());
    if (Q_UNLIKELY(dmabuf->images().constFirst() == EGL_NO_IMAGE_KHR)) {
        qCritical(KWIN_OPENGL) << "Invalid dmabuf-based wl_buffer";
        return false;
    }

    m_texture.reset(new GLTexture(GL_TEXTURE_2D));
    m_texture->setSize(dmabuf->size());
    m_texture->create();
    m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
    m_texture->setFilter(GL_NEAREST);
    m_texture->bind();
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(dmabuf->images().constFirst()));
    m_texture->unbind();
    m_texture->setYInverted(!(dmabuf->flags() & KWaylandServer::LinuxDmabufUnstableV1Interface::YInverted));
    m_bufferType = BufferType::DmaBuf;

    return true;
}

void BasicEGLSurfaceTextureProviderWayland::updateDmabufTexture(KWaylandServer::BufferInterface *buffer)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::DmaBuf)) {
        destroy();
        create();
        return;
    }

    auto dmabuf = static_cast<EglDmabufBuffer *>(buffer->linuxDmabufBuffer());
    m_texture->bind();
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(dmabuf->images().constFirst()));
    m_texture->unbind();
    // The origin in a dmabuf-buffer is at the upper-left corner, so the meaning
    // of Y-inverted is the inverse of OpenGL.
    m_texture->setYInverted(!(dmabuf->flags() & KWaylandServer::LinuxDmabufUnstableV1Interface::YInverted));
}

EGLImageKHR BasicEGLSurfaceTextureProviderWayland::attach(KWaylandServer::BufferInterface *buffer)
{
    const AbstractEglBackendFunctions *funcs = backend()->functions();

    EGLint format;
    funcs->eglQueryWaylandBufferWL(backend()->eglDisplay(), buffer->resource(),
                                   EGL_TEXTURE_FORMAT, &format);
    if (format != EGL_TEXTURE_RGB && format != EGL_TEXTURE_RGBA) {
        qCDebug(KWIN_OPENGL) << "Unsupported texture format: " << format;
        return EGL_NO_IMAGE_KHR;
    }

    EGLint yInverted;
    if (!funcs->eglQueryWaylandBufferWL(backend()->eglDisplay(), buffer->resource(),
                                        EGL_WAYLAND_Y_INVERTED_WL, &yInverted)) {
        // If EGL_WAYLAND_Y_INVERTED_WL is not supported wl_buffer should be treated as
        // if value were EGL_TRUE.
        yInverted = EGL_TRUE;
    }

    const EGLint attribs[] = {
        EGL_WAYLAND_PLANE_WL, 0,
        EGL_NONE
    };
    EGLImageKHR image = eglCreateImageKHR(backend()->eglDisplay(), EGL_NO_CONTEXT,
                                          EGL_WAYLAND_BUFFER_WL,
                                          static_cast<EGLClientBuffer>(buffer->resource()), attribs);
    if (image != EGL_NO_IMAGE_KHR) {
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(image));
        m_texture->setYInverted(yInverted);
    }
    return image;
}

} // namespace KWin
