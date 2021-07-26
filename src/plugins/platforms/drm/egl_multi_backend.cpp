/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "egl_multi_backend.h"
#include <config-kwin.h>
#include "logging.h"
#if HAVE_GBM
#include "egl_gbm_backend.h"
#endif
#if HAVE_EGL_STREAMS
#include "egl_stream_backend.h"
#endif
#include "drm_backend.h"
#include "drm_gpu.h"

namespace KWin
{

EglMultiBackend::EglMultiBackend(DrmBackend *backend, AbstractEglDrmBackend *primaryEglBackend)
    : OpenGLBackend()
    , m_platform(backend)
{
    connect(m_platform, &DrmBackend::gpuAdded, this, &EglMultiBackend::addGpu);
    connect(m_platform, &DrmBackend::gpuRemoved, this, &EglMultiBackend::removeGpu);
    m_backends.append(primaryEglBackend);
    setIsDirectRendering(true);
}

EglMultiBackend::~EglMultiBackend()
{
    for (int i = 1; i < m_backends.count(); i++) {
        delete m_backends[i];
    }
    // delete primary backend last, or this will crash!
    delete m_backends[0];
}

void EglMultiBackend::init()
{
    for (auto b : qAsConst(m_backends)) {
        b->init();
    }
    // we only care about the rendering GPU
    setSupportsSurfacelessContext(m_backends[0]->supportsSurfacelessContext());
    setSupportsBufferAge(m_backends[0]->supportsBufferAge());
    setSupportsPartialUpdate(m_backends[0]->supportsPartialUpdate());
    setSupportsSwapBuffersWithDamage(m_backends[0]->supportsSwapBuffersWithDamage());
    // these are client extensions and the same for all egl backends
    setExtensions(m_backends[0]->extensions());

    m_backends[0]->makeCurrent();
    m_initialized = true;
}

QRegion EglMultiBackend::beginFrame(int screenId)
{
    int internalScreenId;
    AbstractEglBackend *backend = findBackend(screenId, internalScreenId);
    Q_ASSERT(backend != nullptr);
    return backend->beginFrame(internalScreenId);
}

void EglMultiBackend::endFrame(int screenId, const QRegion &damage, const QRegion &damagedRegion)
{
    int internalScreenId;
    AbstractEglBackend *backend = findBackend(screenId, internalScreenId);
    Q_ASSERT(backend != nullptr);
    backend->endFrame(internalScreenId, damage, damagedRegion);
}

bool EglMultiBackend::scanout(int screenId, SurfaceItem *surfaceItem)
{
    int internalScreenId;
    AbstractEglBackend *backend = findBackend(screenId, internalScreenId);
    Q_ASSERT(backend != nullptr);
    return backend->scanout(internalScreenId, surfaceItem);
}

bool EglMultiBackend::makeCurrent()
{
    return m_backends[0]->makeCurrent();
}

void EglMultiBackend::doneCurrent()
{
    m_backends[0]->doneCurrent();
}

PlatformSurfaceTexture *EglMultiBackend::createPlatformSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return m_backends[0]->createPlatformSurfaceTextureInternal(pixmap);
}

PlatformSurfaceTexture *EglMultiBackend::createPlatformSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return m_backends[0]->createPlatformSurfaceTextureWayland(pixmap);
}

QSharedPointer<GLTexture> EglMultiBackend::textureForOutput(AbstractOutput *requestedOutput) const
{
    // this assumes that all outputs are rendered on backend 0
    return m_backends[0]->textureForOutput(requestedOutput);
}

AbstractEglDrmBackend *EglMultiBackend::findBackend(int screenId, int& internalScreenId) const
{
    int screens = 0;
    for (int i = 0; i < m_backends.count(); i++) {
        if (screenId < screens + m_backends[i]->screenCount()) {
            internalScreenId = screenId - screens;
            return m_backends[i];
        }
        screens += m_backends[i]->screenCount();
    }
    qCDebug(KWIN_DRM) << "could not find backend!" << screenId << "/" << screens;
    return nullptr;
}

bool EglMultiBackend::directScanoutAllowed(int screenId) const
{
    int internalScreenId;
    AbstractEglBackend *backend = findBackend(screenId, internalScreenId);
    Q_ASSERT(backend != nullptr);
    return backend->directScanoutAllowed(internalScreenId);
}

void EglMultiBackend::addGpu(DrmGpu *gpu)
{
    AbstractEglDrmBackend *backend;
    if (gpu->useEglStreams()) {
#if HAVE_EGL_STREAMS
        backend = new EglStreamBackend(m_platform, gpu);
#endif
    } else {
#if HAVE_GBM
        backend = new EglGbmBackend(m_platform, gpu);
#endif
    }
    if (backend) {
        if (m_initialized) {
            backend->init();
        }
        m_backends.append(backend);
    }
}

void EglMultiBackend::removeGpu(DrmGpu *gpu)
{
    auto it = std::find_if(m_backends.constBegin(), m_backends.constEnd(), [gpu](auto backend) {
        return backend->gpu() == gpu;
    });
    if (it != m_backends.constEnd()) {
        m_backends.removeOne(*it);
        delete *it;
    }
}

}
