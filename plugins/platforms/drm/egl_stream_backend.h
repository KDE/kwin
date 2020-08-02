/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 NVIDIA Inc.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_EGL_STREAM_BACKEND_H
#define KWIN_EGL_STREAM_BACKEND_H
#include "abstract_egl_backend.h"
#include <KWaylandServer/surface_interface.h>
#include <KWaylandServer/eglstream_controller_interface.h>
#include <wayland-server-core.h>

namespace KWin
{

class DrmBackend;
class DrmOutput;
class DrmBuffer;

/**
 * @brief OpenGL Backend using Egl with an EGLDevice.
 */
class EglStreamBackend : public AbstractEglBackend
{
    Q_OBJECT
public:
    EglStreamBackend(DrmBackend *b);
    ~EglStreamBackend() override;
    void screenGeometryChanged(const QSize &size) override;
    SceneOpenGLTexturePrivate *createBackendTexture(SceneOpenGLTexture *texture) override;
    QRegion prepareRenderingFrame() override;
    void endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    void endRenderingFrameForScreen(int screenId, const QRegion &damage, const QRegion &damagedRegion) override;
    bool usesOverlayWindow() const override;
    bool perScreenRendering() const override;
    QRegion prepareRenderingForScreen(int screenId) override;
    void init() override;

protected:
    void present() override;
    void cleanupSurfaces() override;

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();
    struct StreamTexture 
    {
        EGLStreamKHR stream;
        GLuint texture;
    };
    StreamTexture *lookupStreamTexture(KWaylandServer::SurfaceInterface *surface);
    void attachStreamConsumer(KWaylandServer::SurfaceInterface *surface,
                              void *eglStream,
                              wl_array *attribs);
    struct Output 
    {
        DrmOutput *output = nullptr;
        DrmBuffer *buffer = nullptr;
        EGLSurface eglSurface = EGL_NO_SURFACE;
        EGLStreamKHR eglStream = EGL_NO_STREAM_KHR;
    };
    bool resetOutput(Output &output, DrmOutput *drmOutput);
    bool makeContextCurrent(const Output &output);
    void presentOnOutput(Output &output);
    void cleanupOutput(const Output &output);
    void createOutput(DrmOutput *output);

    DrmBackend *m_backend;
    QVector<Output> m_outputs;
    KWaylandServer::EglStreamControllerInterface *m_eglStreamControllerInterface;
    QHash<KWaylandServer::SurfaceInterface *, StreamTexture> m_streamTextures;

    friend class EglStreamTexture;
};

/**
 * @brief External texture bound to an EGLStreamKHR.
 */
class EglStreamTexture : public AbstractEglTexture
{
public:
    ~EglStreamTexture() override;
    bool loadTexture(WindowPixmap *pixmap) override;
    void updateTexture(WindowPixmap *pixmap) override;

private:
    EglStreamTexture(SceneOpenGLTexture *texture, EglStreamBackend *backend);
    bool acquireStreamFrame(EGLStreamKHR stream);
    void createFbo();
    void copyExternalTexture(GLuint tex);
    bool attachBuffer(KWaylandServer::BufferInterface *buffer);
    EglStreamBackend *m_backend;
    GLuint m_fbo, m_rbo;
    GLenum m_format;
    friend class EglStreamBackend;
};

} // namespace

#endif
