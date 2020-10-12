/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <epoxy/egl.h>
#include "fixqopengl.h"
#include <fixx11h.h>
#include <qpa/qplatformopenglcontext.h>

namespace KWin
{
namespace QPA
{

class EGLPlatformContext : public QPlatformOpenGLContext
{
public:
    EGLPlatformContext(QOpenGLContext *context, EGLDisplay display);
    ~EGLPlatformContext() override;

    bool makeCurrent(QPlatformSurface *surface) override;
    void doneCurrent() override;
    QSurfaceFormat format() const override;
    bool isValid() const override;
    bool isSharing() const override;
    GLuint defaultFramebufferObject(QPlatformSurface *surface) const override;
    QFunctionPointer getProcAddress(const char *procName) override;
    void swapBuffers(QPlatformSurface *surface) override;

    EGLDisplay eglDisplay() const;
    EGLContext eglContext() const;

private:
    void create(const QSurfaceFormat &format, EGLContext shareContext);

    EGLDisplay m_eglDisplay;
    EGLConfig m_config = EGL_NO_CONFIG_KHR;
    EGLContext m_context = EGL_NO_CONTEXT;
    QSurfaceFormat m_format;
};

} // namespace QPA
} // namespace KWin
