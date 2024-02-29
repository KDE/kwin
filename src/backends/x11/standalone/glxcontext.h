/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "opengl/openglcontext.h"
#include "x11_standalone_glx_backend.h"

#include <epoxy/glx.h>

namespace KWin
{

// GLX_MESA_swap_interval
using glXSwapIntervalMESA_func = int (*)(unsigned int interval);
extern glXSwapIntervalMESA_func glXSwapIntervalMESA;

class GlxContext : public OpenGlContext
{
public:
    GlxContext(::Display *display, GLXWindow window, GLXContext handle);
    ~GlxContext() override;

    bool makeCurrent() override;
    void doneCurrent() const override;

    void glXSwapIntervalMESA(unsigned int interval);

    static std::unique_ptr<GlxContext> create(GlxBackend *backend, GLXFBConfig fbconfig, GLXWindow glxWindow);

private:
    ::Display *const m_display;
    const GLXWindow m_window;
    const GLXContext m_handle;
    uint32_t m_vao = 0;
    std::unique_ptr<ShaderManager> m_shaderManager;
    std::unique_ptr<GLVertexBuffer> m_streamingBuffer;
    std::unique_ptr<IndexBuffer> m_indexBuffer;
    glXSwapIntervalMESA_func m_glXSwapIntervalMESA = nullptr;
};

}
