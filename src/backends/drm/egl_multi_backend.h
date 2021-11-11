/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef EGLMULTIBACKEND_H
#define EGLMULTIBACKEND_H

#include "openglbackend.h"

namespace KWin
{

class DrmBackend;
class DrmGpu;
class EglGbmBackend;

class EglMultiBackend : public OpenGLBackend
{
    Q_OBJECT
public:
    EglMultiBackend(DrmBackend *backend, EglGbmBackend *primaryEglBackend);
    ~EglMultiBackend();

    void init() override;

    QRegion beginFrame(AbstractOutput *output) override;
    void endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    bool scanout(AbstractOutput *output, SurfaceItem *surfaceItem) override;

    bool makeCurrent() override;
    void doneCurrent() override;

    SurfaceTexture *createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    SurfaceTexture *createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;
    QSharedPointer<GLTexture> textureForOutput(AbstractOutput *requestedOutput) const override;

    bool directScanoutAllowed(AbstractOutput *output) const override;

public Q_SLOTS:
    void addGpu(DrmGpu *gpu);
    void removeGpu(DrmGpu *gpu);

private:
    DrmBackend *m_platform;
    QVector<EglGbmBackend*> m_backends;
    bool m_initialized = false;

    EglGbmBackend *findBackend(AbstractOutput *output) const;
};

}

#endif // EGLMULTIBACKEND_H
