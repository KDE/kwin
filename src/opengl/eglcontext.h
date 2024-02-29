/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "opengl/gltexture.h"
#include "opengl/openglcontext.h"

#include <QByteArray>
#include <QList>
#include <epoxy/egl.h>

namespace KWin
{

class EglDisplay;
class ShaderManager;
struct DmaBufAttributes;

class KWIN_EXPORT EglContext : public OpenGlContext
{
public:
    EglContext(EglDisplay *display, EGLConfig config, ::EGLContext context);
    ~EglContext() override;

    bool makeCurrent() override;
    bool makeCurrent(EGLSurface surface);
    void doneCurrent() const override;
    std::shared_ptr<GLTexture> importDmaBufAsTexture(const DmaBufAttributes &attributes) const;

    EglDisplay *displayObject() const;
    ::EGLContext handle() const;
    EGLConfig config() const;
    bool isValid() const;

    static std::unique_ptr<EglContext> create(EglDisplay *display, EGLConfig config, ::EGLContext sharedContext);

private:
    static ::EGLContext createContext(EglDisplay *display, EGLConfig config, ::EGLContext sharedContext);

    EglDisplay *const m_display;
    const ::EGLContext m_handle;
    const EGLConfig m_config;
    std::unique_ptr<ShaderManager> m_shaderManager;
    std::unique_ptr<GLVertexBuffer> m_streamingBuffer;
    std::unique_ptr<IndexBuffer> m_indexBuffer;
    uint32_t m_vao = 0;
};

}
