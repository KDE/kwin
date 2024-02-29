/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "eglcontext.h"
#include "core/graphicsbuffer.h"
#include "egldisplay.h"
#include "eglimagetexture.h"
#include "glvertexbuffer_p.h"
#include "opengl/egl_context_attribute_builder.h"
#include "opengl/eglutils_p.h"
#include "opengl/glutils.h"
#include "utils/common.h"
#include "utils/drm_format_helper.h"

#include <QOpenGLContext>
#include <drm_fourcc.h>

namespace KWin
{

std::unique_ptr<EglContext> EglContext::create(EglDisplay *display, EGLConfig config, ::EGLContext sharedContext)
{
    auto handle = createContext(display, config, sharedContext);
    if (!handle) {
        return nullptr;
    }
    if (!eglMakeCurrent(display->handle(), EGL_NO_SURFACE, EGL_NO_SURFACE, handle)) {
        eglDestroyContext(display->handle(), handle);
        return nullptr;
    }
    auto ret = std::make_unique<EglContext>(display, config, handle);
    s_currentContext = ret.get();
    if (!ret->checkSupported()) {
        return nullptr;
    }
    return ret;
}

typedef void (*eglFuncPtr)();
static eglFuncPtr getProcAddress(const char *name)
{
    return eglGetProcAddress(name);
}

EglContext::EglContext(EglDisplay *display, EGLConfig config, ::EGLContext context)
    : OpenGlContext(true)
    , m_display(display)
    , m_handle(context)
    , m_config(config)
    , m_shaderManager(std::make_unique<ShaderManager>())
    , m_streamingBuffer(std::make_unique<GLVertexBuffer>(GLVertexBuffer::Stream))
    , m_indexBuffer(std::make_unique<IndexBuffer>())
{
    glResolveFunctions(&getProcAddress);
    initDebugOutput();
    setShaderManager(m_shaderManager.get());
    setStreamingBuffer(m_streamingBuffer.get());
    setIndexBuffer(m_indexBuffer.get());
    // It is not legal to not have a vertex array object bound in a core context
    // to make code handling old and new OpenGL versions easier, bind a dummy vao that's used for everything
    if (!isOpenGLES() && hasOpenglExtension(QByteArrayLiteral("GL_ARB_vertex_array_object"))) {
        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);
    }
}

EglContext::~EglContext()
{
    makeCurrent();
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
    }
    m_shaderManager.reset();
    m_streamingBuffer.reset();
    m_indexBuffer.reset();
    doneCurrent();
    eglDestroyContext(m_display->handle(), m_handle);
}

bool EglContext::makeCurrent()
{
    return makeCurrent(EGL_NO_SURFACE);
}

bool EglContext::makeCurrent(EGLSurface surface)
{
    if (QOpenGLContext *context = QOpenGLContext::currentContext()) {
        // Workaround to tell Qt that no QOpenGLContext is current
        context->doneCurrent();
    }
    const bool ret = eglMakeCurrent(m_display->handle(), surface, surface, m_handle) == EGL_TRUE;
    if (ret) {
        s_currentContext = this;
    }
    return ret;
}

void EglContext::doneCurrent() const
{
    eglMakeCurrent(m_display->handle(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    s_currentContext = nullptr;
}

EglDisplay *EglContext::displayObject() const
{
    return m_display;
}

::EGLContext EglContext::handle() const
{
    return m_handle;
}

EGLConfig EglContext::config() const
{
    return m_config;
}

bool EglContext::isValid() const
{
    return m_display != nullptr && m_handle != EGL_NO_CONTEXT;
}

static inline bool shouldUseOpenGLES()
{
    if (qstrcmp(qgetenv("KWIN_COMPOSE"), "O2ES") == 0) {
        return true;
    }
    return QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES;
}

::EGLContext EglContext::createContext(EglDisplay *display, EGLConfig config, ::EGLContext sharedContext)
{
    const bool haveRobustness = display->hasExtension(QByteArrayLiteral("EGL_EXT_create_context_robustness"));
    const bool haveCreateContext = display->hasExtension(QByteArrayLiteral("EGL_KHR_create_context"));
    const bool haveContextPriority = display->hasExtension(QByteArrayLiteral("EGL_IMG_context_priority"));
    const bool haveResetOnVideoMemoryPurge = display->hasExtension(QByteArrayLiteral("EGL_NV_robustness_video_memory_purge"));

    std::vector<std::unique_ptr<AbstractOpenGLContextAttributeBuilder>> candidates;
    if (shouldUseOpenGLES()) {
        if (haveCreateContext && haveRobustness && haveContextPriority && haveResetOnVideoMemoryPurge) {
            auto glesRobustPriority = std::make_unique<EglOpenGLESContextAttributeBuilder>();
            glesRobustPriority->setResetOnVideoMemoryPurge(true);
            glesRobustPriority->setVersion(2);
            glesRobustPriority->setRobust(true);
            glesRobustPriority->setHighPriority(true);
            candidates.push_back(std::move(glesRobustPriority));
        }

        if (haveCreateContext && haveRobustness && haveContextPriority) {
            auto glesRobustPriority = std::make_unique<EglOpenGLESContextAttributeBuilder>();
            glesRobustPriority->setVersion(2);
            glesRobustPriority->setRobust(true);
            glesRobustPriority->setHighPriority(true);
            candidates.push_back(std::move(glesRobustPriority));
        }
        if (haveCreateContext && haveRobustness) {
            auto glesRobust = std::make_unique<EglOpenGLESContextAttributeBuilder>();
            glesRobust->setVersion(2);
            glesRobust->setRobust(true);
            candidates.push_back(std::move(glesRobust));
        }
        if (haveContextPriority) {
            auto glesPriority = std::make_unique<EglOpenGLESContextAttributeBuilder>();
            glesPriority->setVersion(2);
            glesPriority->setHighPriority(true);
            candidates.push_back(std::move(glesPriority));
        }
        auto gles = std::make_unique<EglOpenGLESContextAttributeBuilder>();
        gles->setVersion(2);
        candidates.push_back(std::move(gles));
    } else {
        if (haveCreateContext) {
            if (haveRobustness && haveContextPriority && haveResetOnVideoMemoryPurge) {
                auto robustCorePriority = std::make_unique<EglContextAttributeBuilder>();
                robustCorePriority->setResetOnVideoMemoryPurge(true);
                robustCorePriority->setVersion(3, 1);
                robustCorePriority->setRobust(true);
                robustCorePriority->setHighPriority(true);
                candidates.push_back(std::move(robustCorePriority));
            }
            if (haveRobustness && haveContextPriority) {
                auto robustCorePriority = std::make_unique<EglContextAttributeBuilder>();
                robustCorePriority->setVersion(3, 1);
                robustCorePriority->setRobust(true);
                robustCorePriority->setHighPriority(true);
                candidates.push_back(std::move(robustCorePriority));
            }
            if (haveRobustness) {
                auto robustCore = std::make_unique<EglContextAttributeBuilder>();
                robustCore->setVersion(3, 1);
                robustCore->setRobust(true);
                candidates.push_back(std::move(robustCore));
            }
            if (haveContextPriority) {
                auto corePriority = std::make_unique<EglContextAttributeBuilder>();
                corePriority->setVersion(3, 1);
                corePriority->setHighPriority(true);
                candidates.push_back(std::move(corePriority));
            }
            auto core = std::make_unique<EglContextAttributeBuilder>();
            core->setVersion(3, 1);
            candidates.push_back(std::move(core));
        }
        if (haveRobustness && haveCreateContext && haveContextPriority) {
            auto robustPriority = std::make_unique<EglContextAttributeBuilder>();
            robustPriority->setRobust(true);
            robustPriority->setHighPriority(true);
            candidates.push_back(std::move(robustPriority));
        }
        if (haveRobustness && haveCreateContext) {
            auto robust = std::make_unique<EglContextAttributeBuilder>();
            robust->setRobust(true);
            candidates.push_back(std::move(robust));
        }
        candidates.emplace_back(new EglContextAttributeBuilder);
    }

    for (const auto &candidate : candidates) {
        const auto attribs = candidate->build();
        ::EGLContext ctx = eglCreateContext(display->handle(), config, sharedContext, attribs.data());
        if (ctx != EGL_NO_CONTEXT) {
            qCDebug(KWIN_OPENGL) << "Created EGL context with attributes:" << candidate.get();
            return ctx;
        }
    }
    qCCritical(KWIN_OPENGL) << "Create Context failed" << getEglErrorString();
    return EGL_NO_CONTEXT;
}

std::shared_ptr<GLTexture> EglContext::importDmaBufAsTexture(const DmaBufAttributes &attributes) const
{
    EGLImageKHR image = m_display->importDmaBufAsImage(attributes);
    if (image != EGL_NO_IMAGE_KHR) {
        const auto info = FormatInfo::get(attributes.format);
        return EGLImageTexture::create(m_display, image, info ? info->openglFormat : GL_RGBA8, QSize(attributes.width, attributes.height), m_display->isExternalOnly(attributes.format, attributes.modifier));
    } else {
        qCWarning(KWIN_OPENGL) << "Error creating EGLImageKHR: " << getEglErrorString();
        return nullptr;
    }
}
}
