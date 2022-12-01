/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_gbm_surface.h"

#include <errno.h>
#include <gbm.h>

#include "drm_backend.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "kwineglutils_p.h"
#include "kwinglplatform.h"

namespace KWin
{

GbmSurface::GbmSurface(EglGbmBackend *backend, const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers, uint32_t flags, gbm_surface *surface, EGLSurface eglSurface)
    : m_surface(surface)
    , m_eglBackend(backend)
    , m_eglSurface(eglSurface)
    , m_size(size)
    , m_format(format)
    , m_modifiers(modifiers)
    , m_flags(flags)
    , m_fbo(std::make_unique<GLFramebuffer>(0, size))
{
}

GbmSurface::~GbmSurface()
{
    if (m_eglSurface != EGL_NO_SURFACE) {
        eglDestroySurface(m_eglBackend->eglDisplay(), m_eglSurface);
    }
    if (m_surface) {
        gbm_surface_destroy(m_surface);
    }
}

bool GbmSurface::makeContextCurrent() const
{
    if (eglMakeCurrent(m_eglBackend->eglDisplay(), m_eglSurface, m_eglSurface, m_eglBackend->context()) == EGL_FALSE) {
        qCCritical(KWIN_DRM) << "eglMakeCurrent failed:" << getEglErrorString();
        return false;
    }
    if (!GLPlatform::instance()->isGLES()) {
        glDrawBuffer(GL_BACK);
        glReadBuffer(GL_BACK);
    }
    return true;
}

std::shared_ptr<GbmBuffer> GbmSurface::swapBuffers(const QRegion &dirty)
{
    auto error = eglSwapBuffers(m_eglBackend->eglDisplay(), m_eglSurface);
    if (error != EGL_TRUE) {
        qCCritical(KWIN_DRM) << "an error occurred while swapping buffers" << getEglErrorString();
        return nullptr;
    }
    auto bo = gbm_surface_lock_front_buffer(m_surface);
    if (!bo) {
        return nullptr;
    }
    if (m_eglBackend->supportsBufferAge()) {
        eglQuerySurface(m_eglBackend->eglDisplay(), m_eglSurface, EGL_BUFFER_AGE_EXT, &m_bufferAge);
        m_damageJournal.add(dirty);
    }
    return std::make_shared<GbmBuffer>(m_eglBackend->gpu(), bo, shared_from_this());
}

void GbmSurface::releaseBuffer(GbmBuffer *buffer)
{
    gbm_surface_release_buffer(m_surface, buffer->bo());
}

GLFramebuffer *GbmSurface::fbo() const
{
    return m_fbo.get();
}

EGLSurface GbmSurface::eglSurface() const
{
    return m_eglSurface;
}

QSize GbmSurface::size() const
{
    return m_size;
}

uint32_t GbmSurface::format() const
{
    return m_format;
}

QVector<uint64_t> GbmSurface::modifiers() const
{
    return m_modifiers;
}

int GbmSurface::bufferAge() const
{
    return m_bufferAge;
}

QRegion GbmSurface::repaintRegion() const
{
    if (m_eglBackend->supportsBufferAge()) {
        return m_damageJournal.accumulate(m_bufferAge, infiniteRegion());
    } else {
        return infiniteRegion();
    }
}

uint32_t GbmSurface::flags() const
{
    return m_flags;
}

std::variant<std::shared_ptr<GbmSurface>, GbmSurface::Error> GbmSurface::createSurface(EglGbmBackend *backend, const QSize &size, uint32_t format, uint32_t flags, EGLConfig config)
{
    gbm_surface *surface = gbm_surface_create(backend->gpu()->gbmDevice(), size.width(), size.height(), format, flags);
    if (!surface) {
        qCWarning(KWIN_DRM) << "Creating gbm surface failed!" << strerror(errno);
        return Error::Unknown;
    }
    EGLSurface eglSurface = eglCreatePlatformWindowSurfaceEXT(backend->eglDisplay(), config, surface, nullptr);
    if (eglSurface == EGL_NO_SURFACE) {
        qCCritical(KWIN_DRM) << "Creating EGL surface failed!" << getEglErrorString();
        gbm_surface_destroy(surface);
        return Error::Unknown;
    }
    return std::make_shared<GbmSurface>(backend, size, format, QVector<uint64_t>{}, flags, surface, eglSurface);
}

std::variant<std::shared_ptr<GbmSurface>, GbmSurface::Error> GbmSurface::createSurface(EglGbmBackend *backend, const QSize &size, uint32_t format, QVector<uint64_t> modifiers, EGLConfig config)
{
    gbm_surface *surface = gbm_surface_create_with_modifiers(backend->gpu()->gbmDevice(), size.width(), size.height(), format, modifiers.data(), modifiers.size());
    if (!surface) {
        if (errno == ENOSYS) {
            return Error::ModifiersUnsupported;
        } else {
            qCWarning(KWIN_DRM) << "Creating gbm surface failed!" << strerror(errno);
            return Error::Unknown;
        }
    }
    EGLSurface eglSurface = eglCreatePlatformWindowSurfaceEXT(backend->eglDisplay(), config, surface, nullptr);
    if (eglSurface == EGL_NO_SURFACE) {
        qCCritical(KWIN_DRM) << "Creating EGL surface failed!" << getEglErrorString();
        gbm_surface_destroy(surface);
        return Error::Unknown;
    }
    return std::make_shared<GbmSurface>(backend, size, format, modifiers, 0, surface, eglSurface);
}
}
