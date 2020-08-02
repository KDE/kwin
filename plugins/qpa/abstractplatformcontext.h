/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*********************************************************************/
#ifndef KWIN_QPA_ABSTRACTPLATFORMCONTEXT_H
#define KWIN_QPA_ABSTRACTPLATFORMCONTEXT_H

#include <epoxy/egl.h>
#include "fixqopengl.h"
#include <fixx11h.h>
#include <qpa/qplatformopenglcontext.h>

namespace KWin
{
namespace QPA
{

class AbstractPlatformContext : public QPlatformOpenGLContext
{
public:
    AbstractPlatformContext(QOpenGLContext *context, EGLDisplay display, EGLConfig config = nullptr);
    ~AbstractPlatformContext() override;

    void doneCurrent() override;
    QSurfaceFormat format() const override;
    bool isValid() const override;
    QFunctionPointer getProcAddress(const char *procName) override;

protected:
    EGLDisplay eglDisplay() const {
        return m_eglDisplay;
    }
    EGLConfig config() const {
        return m_config;
    }
    bool bindApi();
    EGLContext eglContext() const {
        return m_context;
    }
    void createContext(EGLContext shareContext = EGL_NO_CONTEXT);

private:
    EGLDisplay m_eglDisplay;
    EGLConfig m_config;
    EGLContext m_context = EGL_NO_CONTEXT;
    QSurfaceFormat m_format;
};

}
}

#endif
