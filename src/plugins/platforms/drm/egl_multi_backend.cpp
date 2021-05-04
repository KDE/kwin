/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "egl_multi_backend.h"
#include "logging.h"
#include "egl_gbm_backend.h"
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

SceneOpenGLTexturePrivate *EglMultiBackend::createBackendTexture(SceneOpenGLTexture *texture)
{
    return m_backends[0]->createBackendTexture(texture);
}

QSharedPointer<GLTexture> EglMultiBackend::textureForOutput(AbstractOutput *requestedOutput) const
{
    // this assumes that all outputs are rendered on backend 0
    return m_backends[0]->textureForOutput(requestedOutput);
}

void EglMultiBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
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

void EglMultiBackend::addBackend(AbstractEglDrmBackend *backend)
{
    m_backends.append(backend);
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
    // secondary GPUs are atm guaranteed to be gbm
    auto backend = new EglGbmBackend(m_platform, gpu);
    backend->init();
    m_backends.append(backend);
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
