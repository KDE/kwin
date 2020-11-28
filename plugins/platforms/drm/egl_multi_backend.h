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
public:
    EglMultiBackend(AbstractEglDrmBackend *backend0);
    ~EglMultiBackend();

    void init() override;

    QRegion beginFrame(int screenId) override;
    void endFrame(int screenId, const QRegion &damage, const QRegion &damagedRegion) override;

    bool makeCurrent() override;
    void doneCurrent() override;

    SceneOpenGLTexturePrivate *createBackendTexture(SceneOpenGLTexture *texture) override;
    QSharedPointer<GLTexture> textureForOutput(AbstractOutput *requestedOutput) const override;

    bool usesOverlayWindow() const override;
    bool perScreenRendering() const override;

    void screenGeometryChanged(const QSize &size) override;

    void addBackend(AbstractEglDrmBackend *backend);

protected:
    void present() override;

private:
    QVector<AbstractEglDrmBackend*> m_backends;

    AbstractEglDrmBackend *findBackend(int screenId, int& internalScreenId);

};

}

#endif // EGLMULTIBACKEND_H
