/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef EGLMULTIBACKEND_H
#define EGLMULTIBACKEND_H

#include "abstract_egl_drm_backend.h"

namespace KWin
{

class EglMultiBackend : public OpenGLBackend
{
    Q_OBJECT
public:
    EglMultiBackend(DrmBackend *backend, AbstractEglDrmBackend *primaryEglBackend);
    ~EglMultiBackend();

    void init() override;

    QRegion beginFrame(int screenId) override;
    void endFrame(int screenId, const QRegion &damage, const QRegion &damagedRegion) override;
    bool scanout(int screenId, SurfaceItem *surfaceItem) override;

    bool makeCurrent() override;
    void doneCurrent() override;

    PlatformSurfaceTexture *createPlatformSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    PlatformSurfaceTexture *createPlatformSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;
    QSharedPointer<GLTexture> textureForOutput(AbstractOutput *requestedOutput) const override;

    void screenGeometryChanged(const QSize &size) override;

    bool directScanoutAllowed(int screen) const override;

public Q_SLOTS:
    void addGpu(DrmGpu *gpu);
    void removeGpu(DrmGpu *gpu);

private:
    DrmBackend *m_platform;
    QVector<AbstractEglDrmBackend*> m_backends;
    bool m_initialized = false;

    AbstractEglDrmBackend *findBackend(int screenId, int& internalScreenId) const;
};

}

#endif // EGLMULTIBACKEND_H
