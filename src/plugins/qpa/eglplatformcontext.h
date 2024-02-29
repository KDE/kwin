/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <epoxy/egl.h>

#include <qpa/qplatformopenglcontext.h>

#include <unordered_map>

namespace KWin
{

class GLFramebuffer;
class GLTexture;
class GraphicsBuffer;
class EglDisplay;
class EglContext;

namespace QPA
{

class Window;

class EGLRenderTarget
{
public:
    EGLRenderTarget(GraphicsBuffer *buffer, std::unique_ptr<GLFramebuffer> fbo, std::shared_ptr<GLTexture> texture);
    ~EGLRenderTarget();

    GraphicsBuffer *buffer;
    std::unique_ptr<GLFramebuffer> fbo;
    std::shared_ptr<GLTexture> texture;
};

class EGLPlatformContext : public QObject, public QPlatformOpenGLContext
{
    Q_OBJECT

public:
    EGLPlatformContext(QOpenGLContext *context, EglDisplay *display);
    ~EGLPlatformContext() override;

    bool makeCurrent(QPlatformSurface *surface) override;
    void doneCurrent() override;
    QSurfaceFormat format() const override;
    bool isValid() const override;
    bool isSharing() const override;
    GLuint defaultFramebufferObject(QPlatformSurface *surface) const override;
    QFunctionPointer getProcAddress(const char *procName) override;
    void swapBuffers(QPlatformSurface *surface) override;

private:
    void create(const QSurfaceFormat &format, ::EGLContext shareContext);
    void updateFormatFromContext();

    EglDisplay *const m_eglDisplay;
    QSurfaceFormat m_format;
    EGLConfig m_config = EGL_NO_CONFIG_KHR;
    std::shared_ptr<EglContext> m_eglContext;
    std::unordered_map<GraphicsBuffer *, std::shared_ptr<EGLRenderTarget>> m_renderTargets;
    std::vector<std::shared_ptr<EGLRenderTarget>> m_zombieRenderTargets;
    std::shared_ptr<EGLRenderTarget> m_current;
};

} // namespace QPA
} // namespace KWin
