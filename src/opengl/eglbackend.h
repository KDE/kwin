/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/renderbackend.h"
#include "opengl/eglcontext.h"
#include "opengl/egldisplay.h"
#include "wayland/linuxdmabufv1clientbuffer.h"

#include <QRegion>
#include <memory>

#include <epoxy/egl.h>

namespace KWin
{
class Output;
class GLTexture;
class EglContext;
class EglDisplay;
struct RenderDevice;

struct DmaBufAttributes;

class KWIN_EXPORT EglBackend : public RenderBackend
{
    Q_OBJECT

public:
    virtual void init() = 0;
    CompositingType compositingType() const override final;
    bool checkGraphicsReset() override final;

    EglContext *openglContext() const;
    std::shared_ptr<EglContext> openglContextRef() const;
    EglDisplay *eglDisplayObject() const;

    /**
     * @brief Whether the creation of the Backend failed.
     *
     * The SceneOpenGL should test whether the Backend got constructed correctly. If this method
     * returns @c true, the SceneOpenGL should not try to start the rendering.
     *
     * @return bool @c true if the creation of the Backend failed, @c false otherwise.
     */
    bool isFailed() const
    {
        return m_failed;
    }

    /**
     * The backend specific extensions (e.g. EGL/GLX extensions).
     *
     * Not the OpenGL (ES) extension!
     */
    QList<QByteArray> extensions() const
    {
        return m_extensions;
    }

    /**
     * @returns whether the backend specific extensions contains @p extension.
     */
    bool hasExtension(const QByteArray &extension) const
    {
        return m_extensions.contains(extension);
    }

    bool testImportBuffer(DrmDevice *device, GraphicsBuffer *buffer) override;
    QHash<uint32_t, QList<uint64_t>> supportedFormats() const override;
    DrmDevice *renderDevice() const override;

    QList<LinuxDmaBufV1Feedback::Tranche> tranches() const;

    std::shared_ptr<GLTexture> importDmaBufAsTexture(const DmaBufAttributes &attributes) const;
    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &attributes) const;
    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &attributes, int plane, int format, const QSize &size) const;
    EGLImageKHR importBufferAsImage(GraphicsBuffer *buffer);
    EGLImageKHR importBufferAsImage(GraphicsBuffer *buffer, int plane, int format, const QSize &size);
    EGLImageKHR importBufferAsImage(DrmDevice *device, GraphicsBuffer *buffer);
    EGLImageKHR importBufferAsImage(DrmDevice *device, GraphicsBuffer *buffer, int plane, int format, const QSize &size);

    EglDisplay *eglDisplayForDevice(DrmDevice *device);
    std::shared_ptr<EglContext> eglContextForDevice(DrmDevice *device);
    void resetContextForDevice(DrmDevice *device);

protected:
    EglBackend(const std::shared_ptr<DrmDevice> &renderDevice);

    void cleanup();
    virtual void cleanupSurfaces();
    void setEglDisplay(EglDisplay *display);
    void initClientExtensions();
    void initWayland();
    void updateDmabufFeedback();
    bool hasClientExtension(const QByteArray &ext) const;
    bool isOpenGLES() const;
    bool createContext();

    bool ensureGlobalShareContext(EGLConfig config);
    void destroyGlobalShareContext();
    ::EGLContext createContextInternal(::EGLContext sharedContext);
    void teardown();

    /**
     * @brief Sets the backend initialization to failed.
     *
     * This method should be called by the concrete subclass in case the initialization failed.
     * The given @p reason is logged as a warning.
     *
     * @param reason The reason why the initialization failed.
     */
    void setFailed(const QString &reason);

    EglDisplay *m_display = nullptr;
    std::shared_ptr<EglContext> m_context;
    QList<QByteArray> m_clientExtensions;
    QList<LinuxDmaBufV1Feedback::Tranche> m_tranches;

    /**
     * @brief Whether the initialization failed, of course default to @c false.
     */
    bool m_failed = false;
    QList<QByteArray> m_extensions;

    struct Gpu
    {
        std::weak_ptr<EglContext> context;
        QHash<std::pair<GraphicsBuffer *, int>, EGLImageKHR> importedBuffers;
    };
    std::unordered_map<RenderDevice *, Gpu> m_devices;
    std::unique_ptr<EglDisplay> m_softwareDisplay;

    std::shared_ptr<DrmDevice> m_renderDevice;
};

}
