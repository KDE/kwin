/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "glxcontext.h"
#include "opengl/glvertexbuffer_p.h"
#include "x11_standalone_glx_context_attribute_builder.h"
#include "x11_standalone_logging.h"

#include <QDebug>
#include <QOpenGLContext>
#include <dlfcn.h>

namespace KWin
{

typedef void (*glXFuncPtr)();

static glXFuncPtr getProcAddress(const char *name)
{
    glXFuncPtr ret = nullptr;
#if HAVE_GLX
    ret = glXGetProcAddress((const GLubyte *)name);
#endif
#if HAVE_DL_LIBRARY
    if (ret == nullptr) {
        ret = (glXFuncPtr)dlsym(RTLD_DEFAULT, name);
    }
#endif
    return ret;
}
glXSwapIntervalMESA_func glXSwapIntervalMESA;

GlxContext::GlxContext(::Display *display, GLXWindow window, GLXContext handle)
    : OpenGlContext(false)
    , m_display(display)
    , m_window(window)
    , m_handle(handle)
    , m_shaderManager(std::make_unique<ShaderManager>())
    , m_streamingBuffer(std::make_unique<GLVertexBuffer>(GLVertexBuffer::Stream))
    , m_indexBuffer(std::make_unique<IndexBuffer>())
    , m_glXSwapIntervalMESA((glXSwapIntervalMESA_func)getProcAddress("glXSwapIntervalMESA"))
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

GlxContext::~GlxContext()
{
    makeCurrent();
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
    }
    m_shaderManager.reset();
    m_streamingBuffer.reset();
    m_indexBuffer.reset();
    glXDestroyContext(m_display, m_handle);
}

bool GlxContext::makeCurrent()
{
    if (QOpenGLContext *context = QOpenGLContext::currentContext()) {
        // Workaround to tell Qt that no QOpenGLContext is current
        context->doneCurrent();
    }
    const bool ret = glXMakeCurrent(m_display, m_window, m_handle);
    if (ret) {
        s_currentContext = this;
    }
    return ret;
}

void GlxContext::doneCurrent() const
{
    glXMakeCurrent(m_display, None, nullptr);
    s_currentContext = nullptr;
}

void GlxContext::glXSwapIntervalMESA(unsigned int interval)
{
    if (m_glXSwapIntervalMESA) {
        m_glXSwapIntervalMESA(interval);
    }
}

std::unique_ptr<GlxContext> GlxContext::create(GlxBackend *backend, GLXFBConfig fbconfig, GLXWindow glxWindow)
{
    QOpenGLContext *qtGlobalShareContext = QOpenGLContext::globalShareContext();
    GLXContext globalShareContext = nullptr;
    if (qtGlobalShareContext) {
        qDebug(KWIN_X11STANDALONE) << "Global share context format:" << qtGlobalShareContext->format();
        const auto nativeHandle = qtGlobalShareContext->nativeInterface<QNativeInterface::QGLXContext>();
        if (nativeHandle) {
            globalShareContext = nativeHandle->nativeContext();
        } else {
            qCDebug(KWIN_X11STANDALONE) << "Invalid QOpenGLContext::globalShareContext()";
            return nullptr;
        }
    }
    if (!globalShareContext) {
        qCWarning(KWIN_X11STANDALONE) << "QOpenGLContext::globalShareContext() is required";
        return nullptr;
    }

    GLXContext handle = nullptr;

    // Use glXCreateContextAttribsARB() when it's available
    if (backend->hasExtension(QByteArrayLiteral("GLX_ARB_create_context"))) {
        const bool have_robustness = backend->hasExtension(QByteArrayLiteral("GLX_ARB_create_context_robustness"));
        const bool haveVideoMemoryPurge = backend->hasExtension(QByteArrayLiteral("GLX_NV_robustness_video_memory_purge"));

        std::vector<GlxContextAttributeBuilder> candidates;
        // core
        if (have_robustness) {
            if (haveVideoMemoryPurge) {
                GlxContextAttributeBuilder purgeMemoryCore;
                purgeMemoryCore.setVersion(3, 1);
                purgeMemoryCore.setRobust(true);
                purgeMemoryCore.setResetOnVideoMemoryPurge(true);
                candidates.emplace_back(std::move(purgeMemoryCore));
            }
            GlxContextAttributeBuilder robustCore;
            robustCore.setVersion(3, 1);
            robustCore.setRobust(true);
            candidates.emplace_back(std::move(robustCore));
        }
        GlxContextAttributeBuilder core;
        core.setVersion(3, 1);
        candidates.emplace_back(std::move(core));
        // legacy
        if (have_robustness) {
            if (haveVideoMemoryPurge) {
                GlxContextAttributeBuilder purgeMemoryLegacy;
                purgeMemoryLegacy.setRobust(true);
                purgeMemoryLegacy.setResetOnVideoMemoryPurge(true);
                candidates.emplace_back(std::move(purgeMemoryLegacy));
            }
            GlxContextAttributeBuilder robustLegacy;
            robustLegacy.setRobust(true);
            candidates.emplace_back(std::move(robustLegacy));
        }
        GlxContextAttributeBuilder legacy;
        legacy.setVersion(2, 1);
        candidates.emplace_back(std::move(legacy));
        for (auto it = candidates.begin(); it != candidates.end(); it++) {
            const auto attribs = it->build();
            handle = glXCreateContextAttribsARB(backend->display(), fbconfig, globalShareContext, true, attribs.data());
            if (handle) {
                qCDebug(KWIN_X11STANDALONE) << "Created GLX context with attributes:" << &(*it);
                break;
            }
        }
    }
    if (!handle) {
        handle = glXCreateNewContext(backend->display(), fbconfig, GLX_RGBA_TYPE, globalShareContext, true);
    }
    if (!handle) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to create an OpenGL context.";
        return nullptr;
    }
    // KWin doesn't support indirect rendering
    if (!glXIsDirect(backend->display(), handle)) {
        return nullptr;
    }
    if (!glXMakeCurrent(backend->display(), glxWindow, handle)) {
        glXDestroyContext(backend->display(), handle);
        return nullptr;
    }
    auto ret = std::make_unique<GlxContext>(backend->display(), glxWindow, handle);
    s_currentContext = ret.get();
    if (!ret->checkSupported()) {
        return nullptr;
    }
    return ret;
}

}
