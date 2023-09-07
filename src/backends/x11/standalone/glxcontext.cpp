/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "glxcontext.h"
#include "x11_standalone_glx_context_attribute_builder.h"
#include "x11_standalone_logging.h"

#include <QDebug>
#include <QOpenGLContext>

namespace KWin
{

GlxContext::GlxContext(Display *display, GLXWindow window, GLXContext handle)
    : m_display(display)
    , m_window(window)
    , m_handle(handle)
{
}

GlxContext::~GlxContext()
{
    glXDestroyContext(m_display, m_handle);
}

bool GlxContext::makeCurrent() const
{
    if (QOpenGLContext *context = QOpenGLContext::currentContext()) {
        // Workaround to tell Qt that no QOpenGLContext is current
        context->doneCurrent();
    }
    return glXMakeCurrent(m_display, m_window, m_handle);
}

bool GlxContext::doneCurrent() const
{
    return glXMakeCurrent(m_display, None, nullptr);
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
    return std::make_unique<GlxContext>(backend->display(), glxWindow, handle);
}

}
