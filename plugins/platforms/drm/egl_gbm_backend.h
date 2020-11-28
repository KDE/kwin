/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_EGL_GBM_BACKEND_H
#define KWIN_EGL_GBM_BACKEND_H
#include "abstract_egl_drm_backend.h"

#include <memory>

struct gbm_surface;
struct gbm_bo;

namespace KWin
{
class AbstractOutput;
class DrmBuffer;
class DrmSurfaceBuffer;
class DrmOutput;
class GbmSurface;

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 */
class EglGbmBackend : public AbstractEglDrmBackend
{
    Q_OBJECT
public:
    EglGbmBackend(DrmBackend *drmBackend, DrmGpu *gpu);

    SceneOpenGLTexturePrivate *createBackendTexture(SceneOpenGLTexture *texture) override;
    QRegion beginFrame(int screenId) override;
    void endFrame(int screenId, const QRegion &damage, const QRegion &damagedRegion) override;
    void init() override;

    QSharedPointer<GLTexture> textureForOutput(AbstractOutput *requestedOutput) const override;

    int screenCount() const override {
        return m_outputs.count();
    }

    void addSecondaryGpuOutput(AbstractOutput *output) override;
    int getDmabufForSecondaryGpuOutput(AbstractOutput *output, uint32_t *format, uint32_t *stride) override;
    void cleanupDmabufForSecondaryGpuOutput(AbstractOutput *output) override;
    void removeSecondaryGpuOutput(AbstractOutput *output) override;
    QRegion beginFrameForSecondaryGpu(AbstractOutput *output) override;
    void renderFramebufferToSurface(AbstractOutput *output) override;

protected:
    void present() override;
    void cleanupSurfaces() override;
    void aboutToStartPainting(int screenId, const QRegion &damage) override;

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();

    struct Output {
        DrmOutput *output = nullptr;
        DrmSurfaceBuffer *buffer = nullptr;
        std::shared_ptr<GbmSurface> gbmSurface;
        EGLSurface eglSurface = EGL_NO_SURFACE;
        int bufferAge = 0;
        /**
         * @brief The damage history for the past 10 frames.
         */
        QList<QRegion> damageHistory;

        struct {
            GLuint framebuffer = 0;
            GLuint texture = 0;
            std::shared_ptr<GLVertexBuffer> vbo;
        } render;

        bool onSecondaryGPU = false;
        int dmabufFd = 0;
        gbm_bo *secondaryGbmBo = nullptr;
        gbm_bo *importedGbmBo = nullptr;
    };

    void createOutput(DrmOutput *drmOutput);
    bool resetOutput(Output &output, DrmOutput *drmOutput);
    std::shared_ptr<GbmSurface> createGbmSurface(const QSize &size, const bool linear) const;
    EGLSurface createEglSurface(std::shared_ptr<GbmSurface> gbmSurface) const;

    bool makeContextCurrent(const Output &output) const;
    void setViewport(const Output &output) const;

    bool resetFramebuffer(Output &output);
    void initRenderTarget(Output &output);

    void prepareRenderFramebuffer(const Output &output) const;
    void renderFramebufferToSurface(Output &output);
    QRegion prepareRenderingForOutput(const Output &output) const;

    void presentOnOutput(Output &output, const QRegion &damagedRegion);

    void removeOutput(DrmOutput *drmOutput);
    void cleanupOutput(Output &output);
    void cleanupFramebuffer(Output &output);

    QVector<Output> m_outputs;
    QVector<Output> m_secondaryGpuOutputs;

    friend class EglGbmTexture;
};

/**
 * @brief Texture using an EGLImageKHR.
 */
class EglGbmTexture : public AbstractEglTexture
{
public:
    ~EglGbmTexture() override;

private:
    friend class EglGbmBackend;
    EglGbmTexture(SceneOpenGLTexture *texture, EglGbmBackend *backend);
};

} // namespace

#endif
