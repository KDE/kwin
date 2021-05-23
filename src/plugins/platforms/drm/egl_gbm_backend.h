/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_EGL_GBM_BACKEND_H
#define KWIN_EGL_GBM_BACKEND_H
#include "abstract_egl_drm_backend.h"

#include <QSharedPointer>

struct gbm_surface;
struct gbm_bo;

namespace KWaylandServer
{
class BufferInterface;
class SurfaceInterface;
}

namespace KWin
{
class AbstractOutput;
class DrmBuffer;
class DrmGbmBuffer;
class DrmOutput;
class GbmSurface;
class GbmBuffer;
class DumbSwapchain;

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 */
class EglGbmBackend : public AbstractEglDrmBackend
{
    Q_OBJECT
public:
    EglGbmBackend(DrmBackend *drmBackend, DrmGpu *gpu);
    ~EglGbmBackend() override;

    SceneOpenGLTexturePrivate *createBackendTexture(SceneOpenGLTexture *texture) override;
    QRegion beginFrame(int screenId) override;
    void endFrame(int screenId, const QRegion &damage, const QRegion &damagedRegion) override;
    void init() override;
    bool scanout(int screenId, SurfaceItem *surfaceItem) override;

    QSharedPointer<GLTexture> textureForOutput(AbstractOutput *requestedOutput) const override;

    int screenCount() const override {
        return m_outputs.count();
    }

    bool addOutput(DrmOutput *output) override;
    void removeOutput(DrmOutput *output) override;
    bool swapBuffers(DrmOutput *output) override;
    bool exportFramebuffer(DrmOutput *output, void *data, const QSize &size, uint32_t stride) override;
    int exportFramebufferAsDmabuf(DrmOutput *output, uint32_t *format, uint32_t *stride) override;
    QRegion beginFrameForSecondaryGpu(DrmOutput *output) override;

    bool directScanoutAllowed(int screen) const override;

protected:
    void cleanupSurfaces() override;
    void aboutToStartPainting(int screenId, const QRegion &damage) override;

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();

    enum class ImportMode {
        Dmabuf,
        DumbBuffer
    };
    struct Output {
        DrmOutput *output = nullptr;
        QSharedPointer<DrmBuffer> buffer;
        QSharedPointer<GbmBuffer> secondaryBuffer;
        QSharedPointer<GbmSurface> gbmSurface;
        EGLSurface eglSurface = EGL_NO_SURFACE;
        int bufferAge = 0;
        /**
         * @brief The damage history for the past 10 frames.
         */
        QList<QRegion> damageHistory;

        struct {
            GLuint framebuffer = 0;
            GLuint texture = 0;
            QSharedPointer<GLVertexBuffer> vbo;
        } render;

        KWaylandServer::SurfaceInterface *surfaceInterface = nullptr;
        ImportMode importMode = ImportMode::Dmabuf;
        QSharedPointer<DumbSwapchain> importSwapchain;
    };

    bool resetOutput(Output &output, DrmOutput *drmOutput);
    EGLSurface createEglSurface(QSharedPointer<GbmSurface> gbmSurface) const;

    bool makeContextCurrent(const Output &output) const;
    void setViewport(const Output &output) const;

    bool resetFramebuffer(Output &output);
    void initRenderTarget(Output &output);

    void prepareRenderFramebuffer(const Output &output) const;
    void renderFramebufferToSurface(Output &output);
    QRegion prepareRenderingForOutput(Output &output) const;
    void importFramebuffer(Output &output) const;

    bool presentOnOutput(Output &output, const QRegion &damagedRegion);
    bool directScanoutActive(const Output &output);

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
