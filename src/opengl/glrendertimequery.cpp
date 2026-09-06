/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "glrendertimequery.h"
#include "opengl/eglcontext.h"
#include "utils/common.h"

namespace KWin
{

std::unique_ptr<GLRenderTimeQuery> GLRenderTimeQuery::begin(const std::shared_ptr<EglContext> &context)
{
    if (!context->supportsTimerQueries()) {
        return nullptr;
    }
    GLuint query = 0;
    glGenQueries(1, &query);
    if (!query) {
        return nullptr;
    }
    return std::make_unique<GLRenderTimeQuery>(context, query);
}

std::unique_ptr<GLRenderTimeQuery> GLRenderTimeQuery::begin()
{
    return begin(EglContext::currentContext()->shared_from_this());
}

GLRenderTimeQuery::GLRenderTimeQuery(const std::shared_ptr<EglContext> &context, GLuint query)
    : m_context(context)
{
    m_gpuProbe.query = query;
    GLint64 start = 0;
    glGetInteger64v(GL_TIMESTAMP, &start);
    m_gpuProbe.start = std::chrono::nanoseconds(start);
    m_cpuStart = std::chrono::steady_clock::now();
}

GLRenderTimeQuery::~GLRenderTimeQuery()
{
    if (!m_gpuProbe.query) {
        return;
    }
    const auto previousContext = EglContext::currentContext();
    const auto context = m_context.lock();
    if (!context || !context->makeCurrent()) {
        qCWarning(KWIN_OPENGL, "Could not delete render time query because no context is current");
        return;
    }
    glDeleteQueries(1, &m_gpuProbe.query);
    if (previousContext && previousContext != context.get()) {
        (void)previousContext->makeCurrent();
    }
}

void GLRenderTimeQuery::end()
{
    m_hasResult = true;
    glQueryCounter(m_gpuProbe.query, GL_TIMESTAMP);
}

std::optional<RenderTimeSpan> GLRenderTimeQuery::query()
{
    if (m_result) {
        return m_result;
    }
    Q_ASSERT(m_hasResult);
    if (m_gpuProbe.query) {
        const auto previousContext = EglContext::currentContext();
        const auto context = m_context.lock();
        if (!context || !context->makeCurrent()) {
            return std::nullopt;
        }
        GLint64 end = 0;
        glGetQueryObjecti64v(m_gpuProbe.query, GL_QUERY_RESULT, &end);
        m_gpuProbe.end = std::chrono::nanoseconds(end);
        if (previousContext && previousContext != context.get()) {
            (void)previousContext->makeCurrent();
        }
    }

    m_result = RenderTimeSpan{
        .start = m_cpuStart,
        .end = m_cpuStart + (m_gpuProbe.end - m_gpuProbe.start),
    };
    return m_result;
}

}
