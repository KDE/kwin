/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/renderbackend.h"

#include <QRegion>
#include <memory>

namespace KWin
{
class Output;
class OpenGLBackend;
class GLTexture;
class OpenGlContext;
class EglDisplay;

/**
 * @brief The OpenGLBackend creates and holds the OpenGL context and is responsible for Texture from Pixmap.
 *
 * The OpenGLBackend is an abstract base class used by the SceneOpenGL to abstract away the differences
 * between various OpenGL windowing systems such as GLX and EGL.
 *
 * A concrete implementation has to create and release the OpenGL context in a way so that the
 * SceneOpenGL does not have to care about it.
 *
 * In addition a major task for this class is to generate the SceneOpenGLTexturePrivate which is
 * able to perform the texture from pixmap operation in the given backend.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 */
class KWIN_EXPORT OpenGLBackend : public RenderBackend
{
    Q_OBJECT

public:
    OpenGLBackend();
    virtual ~OpenGLBackend();

    virtual void init() = 0;
    CompositingType compositingType() const override final;
    bool checkGraphicsReset() override final;

    virtual bool makeCurrent() = 0;
    virtual void doneCurrent() = 0;
    virtual OpenGlContext *openglContext() const = 0;

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

    bool supportsBufferAge() const
    {
        return m_haveBufferAge;
    }

    bool supportsNativeFence() const
    {
        return m_haveNativeFence;
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

    virtual EglDisplay *eglDisplayObject() const;

protected:
    /**
     * @brief Sets the backend initialization to failed.
     *
     * This method should be called by the concrete subclass in case the initialization failed.
     * The given @p reason is logged as a warning.
     *
     * @param reason The reason why the initialization failed.
     */
    void setFailed(const QString &reason);

    void setSupportsBufferAge(bool value)
    {
        m_haveBufferAge = value;
    }

    void setSupportsNativeFence(bool value)
    {
        m_haveNativeFence = value;
    }

    /**
     * Sets the platform-specific @p extensions.
     *
     * These are the EGL/GLX extensions, not the OpenGL extensions
     */
    void setExtensions(const QList<QByteArray> &extensions)
    {
        m_extensions = extensions;
    }

private:
    /**
     * @brief Whether the backend supports GLX_EXT_buffer_age / EGL_EXT_buffer_age.
     */
    bool m_haveBufferAge;
    /**
     * @brief Whether the backend supports EGL_ANDROID_native_fence_sync.
     */
    bool m_haveNativeFence = false;
    /**
     * @brief Whether the initialization failed, of course default to @c false.
     */
    bool m_failed;
    QList<QByteArray> m_extensions;
};

}
