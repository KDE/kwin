/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 NVIDIA Inc.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_EGL_STREAM_BACKEND_H
#define KWIN_EGL_STREAM_BACKEND_H
#include "abstract_egl_drm_backend.h"

namespace KWin
{

class DrmOutput;
class DrmBuffer;

/**
 * @brief OpenGL Backend using Egl with an EGLDevice.
 */
class EglStreamBackend : public AbstractEglDrmBackend
{
    Q_OBJECT
public:
    EglStreamBackend(DrmBackend *b, DrmGpu *gpu);
    SceneOpenGLTexturePrivate *createBackendTexture(SceneOpenGLTexture *texture) override;
    QRegion beginFrame(int screenId) override;
    void endFrame(int screenId, const QRegion &damage, const QRegion &damagedRegion) override;
    void init() override;

    int screenCount() const override {
        return m_outputs.count();
    }

    void addOutput(DrmOutput *output) override;
    void removeOutput(DrmOutput *output) override;

protected:
    void cleanupSurfaces() override;

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();
    struct Output 
    {
        DrmOutput *output = nullptr;
        DrmBuffer *buffer = nullptr;
        EGLSurface eglSurface = EGL_NO_SURFACE;
        EGLStreamKHR eglStream = EGL_NO_STREAM_KHR;
    };
    bool resetOutput(Output &output, DrmOutput *drmOutput);
    bool makeContextCurrent(const Output &output);
    bool presentOnOutput(Output &output);
    void cleanupOutput(const Output &output);

    QVector<Output> m_outputs;
};

/**
 * @brief External texture bound to an EGLStreamKHR.
 */
class EglStreamTexture : public AbstractEglTexture
{
public:
    ~EglStreamTexture() override;

private:
    EglStreamTexture(SceneOpenGLTexture *texture, EglStreamBackend *backend);
    friend class EglStreamBackend;
};

} // namespace

#endif
