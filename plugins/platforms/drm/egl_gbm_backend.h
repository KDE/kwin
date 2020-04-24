/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_EGL_GBM_BACKEND_H
#define KWIN_EGL_GBM_BACKEND_H
#include "abstract_egl_backend.h"

#include <memory>

struct gbm_surface;

namespace KWin
{
class AbstractOutput;
class DrmBackend;
class DrmBuffer;
class DrmSurfaceBuffer;
class DrmOutput;
class GbmSurface;

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 */
class EglGbmBackend : public AbstractEglBackend
{
    Q_OBJECT
public:
    EglGbmBackend(DrmBackend *drmBackend);
    ~EglGbmBackend() override;
    void screenGeometryChanged(const QSize &size) override;
    SceneOpenGLTexturePrivate *createBackendTexture(SceneOpenGLTexture *texture) override;
    QRegion prepareRenderingFrame() override;
    void endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    void endRenderingFrameForScreen(int screenId, const QRegion &damage, const QRegion &damagedRegion) override;
    bool usesOverlayWindow() const override;
    bool perScreenRendering() const override;
    QRegion prepareRenderingForScreen(int screenId) override;
    void init() override;

    QSharedPointer<GLTexture> textureForOutput(AbstractOutput *requestedOutput) const override;

protected:
    void present() override;
    void cleanupSurfaces() override;
    void aboutToStartPainting(const QRegion &damage) override;

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
    };

    void createOutput(DrmOutput *drmOutput);
    bool resetOutput(Output &output, DrmOutput *drmOutput);
    std::shared_ptr<GbmSurface> createGbmSurface(const QSize &size) const;
    EGLSurface createEglSurface(std::shared_ptr<GbmSurface> gbmSurface) const;

    bool makeContextCurrent(const Output &output) const;
    void setViewport(const Output &output) const;

    bool resetFramebuffer(Output &output);
    void initRenderTarget(Output &output);

    void prepareRenderFramebuffer(const Output &output) const;
    void renderFramebufferToSurface(Output &output);

    void presentOnOutput(Output &output, const QRegion &damagedRegion);

    void removeOutput(DrmOutput *drmOutput);
    void cleanupOutput(Output &output);
    void cleanupFramebuffer(Output &output);

    DrmBackend *m_backend;
    QVector<Output> m_outputs;
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
