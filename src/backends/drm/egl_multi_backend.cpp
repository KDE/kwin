/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "egl_multi_backend.h"
#include <config-kwin.h>
#include "logging.h"
#include "egl_gbm_backend.h"
#include "drm_backend.h"
#include "drm_gpu.h"

namespace KWin
{

EglMultiBackend::EglMultiBackend(DrmBackend *backend, EglGbmBackend *primaryEglBackend)
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
    setSupportsBufferAge(m_backends[0]->supportsBufferAge());
    setSupportsPartialUpdate(m_backends[0]->supportsPartialUpdate());
    setSupportsSwapBuffersWithDamage(m_backends[0]->supportsSwapBuffersWithDamage());
    // these are client extensions and the same for all egl backends
    setExtensions(m_backends[0]->extensions());

    m_backends[0]->makeCurrent();
    m_initialized = true;
}

QRegion EglMultiBackend::beginFrame(AbstractOutput *output)
{
    return findBackend(output)->beginFrame(output);
}

void EglMultiBackend::endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    findBackend(output)->endFrame(output, renderedRegion, damagedRegion);
}

bool EglMultiBackend::scanout(AbstractOutput *output, SurfaceItem *surfaceItem)
{
    return findBackend(output)->scanout(output, surfaceItem);
}

bool EglMultiBackend::makeCurrent()
{
    return m_backends[0]->makeCurrent();
}

void EglMultiBackend::doneCurrent()
{
    m_backends[0]->doneCurrent();
}

SurfaceTexture *EglMultiBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return m_backends[0]->createSurfaceTextureInternal(pixmap);
}

SurfaceTexture *EglMultiBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return m_backends[0]->createSurfaceTextureWayland(pixmap);
}

QSharedPointer<GLTexture> EglMultiBackend::textureForOutput(AbstractOutput *requestedOutput) const
{
    // this assumes that all outputs are rendered on backend 0
    return m_backends[0]->textureForOutput(requestedOutput);
}

EglGbmBackend *EglMultiBackend::findBackend(AbstractOutput *output) const
{
    for (int i = 1; i < m_backends.count(); i++) {
        if (m_backends[i]->hasOutput(output)) {
            return m_backends[i];
        }
    }
    return m_backends[0];
}

bool EglMultiBackend::directScanoutAllowed(AbstractOutput *output) const
{
    return findBackend(output)->directScanoutAllowed(output);
}

void EglMultiBackend::addGpu(DrmGpu *gpu)
{
    EglGbmBackend *backend= new EglGbmBackend(m_platform, gpu);
    if (m_initialized) {
        backend->init();
    }
    m_backends.append(backend);
}

void EglMultiBackend::removeGpu(DrmGpu *gpu)
{
    auto it = std::find_if(m_backends.begin(), m_backends.end(), [gpu](const auto &backend) {
        return backend->gpu() == gpu;
    });
    if (it != m_backends.end()) {
        delete *it;
        m_backends.erase(it);
    }
}

}
