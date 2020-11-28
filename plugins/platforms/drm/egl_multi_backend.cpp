/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "egl_multi_backend.h"
#include "logging.h"

namespace KWin
{

EglMultiBackend::EglMultiBackend(AbstractEglDrmBackend *backend0) : OpenGLBackend()
{
    m_backends.append(backend0);
    setIsDirectRendering(true);
    setSyncsToVBlank(true);
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
    // set all the values:
    // if any block, set it to be blocking
    setBlocksForRetrace(false);
    // if any don't support it set it to not supported
    setSupportsBufferAge(true);
    setSupportsPartialUpdate(true);
    setSupportsSwapBuffersWithDamage(true);
    for (auto b : qAsConst(m_backends)) {
        if (b->blocksForRetrace()) {
            setBlocksForRetrace(true);
        }
        if (!b->supportsBufferAge()) {
            setSupportsBufferAge(false);
        }
        if (!b->supportsPartialUpdate()) {
            setSupportsPartialUpdate(false);
        }
        if (!b->supportsSwapBuffersWithDamage()) {
            setSupportsSwapBuffersWithDamage(false);
        }
    }
    // we only care about the rendering GPU here
    setSupportsSurfacelessContext(m_backends[0]->supportsSurfacelessContext());
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
    // this assumes that the wrong backends return {}
    for (auto backend : qAsConst(m_backends)) {
        auto texture = backend->textureForOutput(requestedOutput);
        if (!texture.isNull()) {
            return texture;
        }
    }
    return {};
}

bool EglMultiBackend::usesOverlayWindow() const
{
    return false;
}

bool EglMultiBackend::perScreenRendering() const
{
    return true;
}

void EglMultiBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
}

void EglMultiBackend::present()
{
    Q_UNREACHABLE();
}

AbstractEglDrmBackend *EglMultiBackend::findBackend(int screenId, int& internalScreenId)
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

}
