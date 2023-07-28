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

namespace KWin
{

class EglDisplay;
class EglContext;

namespace QPA
{

class EGLPlatformContext : public QPlatformOpenGLContext
{
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

    EglDisplay *m_eglDisplay = nullptr;
    EGLConfig m_config = EGL_NO_CONFIG_KHR;
    std::unique_ptr<EglContext> m_eglContext;
    QSurfaceFormat m_format;
};

} // namespace QPA
} // namespace KWin
