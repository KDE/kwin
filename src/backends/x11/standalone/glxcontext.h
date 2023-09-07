/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "libkwineffects/openglcontext.h"
#include "x11_standalone_glx_backend.h"

#include <epoxy/glx.h>

namespace KWin
{

class GlxContext : public OpenGlContext
{
public:
    GlxContext(Display *display, GLXWindow window, GLXContext handle);
    ~GlxContext() override;

    bool makeCurrent() const;
    bool doneCurrent() const;

    static std::unique_ptr<GlxContext> create(GlxBackend *backend, GLXFBConfig fbconfig, GLXWindow glxWindow);

private:
    Display *const m_display;
    const GLXWindow m_window;
    const GLXContext m_handle;
};

}
