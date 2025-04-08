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

    /**
     * Copy a region of pixels from the current read to the current draw buffer
     */
    void copyPixels(const QRegion &region, const QSize &screenSize);

    virtual std::pair<std::shared_ptr<GLTexture>, ColorDescription> textureForOutput(Output *output) const;

    bool testImportBuffer(GraphicsBuffer *buffer) override;
    QHash<uint32_t, QList<uint64_t>> supportedFormats() const override;

    QList<LinuxDmaBufV1Feedback::Tranche> tranches() const;

    std::shared_ptr<GLTexture> importDmaBufAsTexture(const DmaBufAttributes &attributes) const;
    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &attributes) const;
    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &attributes, int plane, int format, const QSize &size) const;
    EGLImageKHR importBufferAsImage(GraphicsBuffer *buffer);
    EGLImageKHR importBufferAsImage(GraphicsBuffer *buffer, int plane, int format, const QSize &size);

    std::unique_ptr<SurfaceTexture> createSurfaceTextureWayland(SurfacePixmap *pixmap) override;

protected:
    EglBackend();

    void cleanup();
    virtual void cleanupSurfaces();
    void setEglDisplay(EglDisplay *display);
    void initClientExtensions();
    void initWayland();
    bool hasClientExtension(const QByteArray &ext) const;
    bool isOpenGLES() const;
    bool createContext(EGLConfig config);

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
    QHash<std::pair<GraphicsBuffer *, int>, EGLImageKHR> m_importedBuffers;

    /**
     * @brief Whether the initialization failed, of course default to @c false.
     */
    bool m_failed = false;
    QList<QByteArray> m_extensions;
};

}
