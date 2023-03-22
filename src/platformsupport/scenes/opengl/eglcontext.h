/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/dmabufattributes.h"
#include "kwin_export.h"
#include "kwingltexture.h"
#include "openglcontext.h"

#include <QByteArray>
#include <QList>
#include <epoxy/egl.h>

namespace KWin
{

class EglDisplay;

class KWIN_EXPORT EglContext : public OpenGlContext
{
public:
    EglContext(EglDisplay *display, EGLConfig config, ::EGLContext context);
    ~EglContext() override;

    bool makeCurrent(EGLSurface surface = EGL_NO_SURFACE) const;
    void doneCurrent() const;
    std::shared_ptr<GLTexture> importDmaBufAsTexture(const DmaBufAttributes &attributes) const;

    EglDisplay *displayObject() const;
    ::EGLContext handle() const;
    EGLConfig config() const;
    bool isValid() const;

    static std::unique_ptr<EglContext> create(EglDisplay *display, EGLConfig config, ::EGLContext sharedContext);

private:
    static ::EGLContext createContext(EglDisplay *display, EGLConfig config, ::EGLContext sharedContext);

    EglDisplay *m_display = nullptr;
    ::EGLContext m_handle = EGL_NO_CONTEXT;
    EGLConfig m_config = EGL_NO_CONFIG_KHR;
};

}
