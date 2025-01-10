/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "glrendertimequery.h"
#include "opengl/glplatform.h"
#include "utils/common.h"

namespace KWin
{

GLRenderTimeQuery::GLRenderTimeQuery(const std::shared_ptr<OpenGlContext> &context)
    : m_context(context)
{
    if (context->supportsTimerQueries()) {
        glGenQueries(1, &m_gpuProbe.query);
    }
}

GLRenderTimeQuery::~GLRenderTimeQuery()
{
    if (!m_gpuProbe.query) {
        return;
    }
    const auto previousContext = OpenGlContext::currentContext();
    const auto context = m_context.lock();
    if (!context || !context->makeCurrent()) {
        qCWarning(KWIN_OPENGL, "Could not delete render time query because no context is current");
        return;
    }
    glDeleteQueries(1, &m_gpuProbe.query);
    if (previousContext && previousContext != context.get()) {
        previousContext->makeCurrent();
    }
}

void GLRenderTimeQuery::begin()
{
    if (m_gpuProbe.query) {
        GLint64 start = 0;
        glGetInteger64v(GL_TIMESTAMP, &start);
        m_gpuProbe.start = std::chrono::nanoseconds(start);
    }
    m_cpuProbe.start = std::chrono::steady_clock::now();
}

void GLRenderTimeQuery::end()
{
    m_hasResult = true;

    if (m_gpuProbe.query) {
        glQueryCounter(m_gpuProbe.query, GL_TIMESTAMP);
    }
    m_cpuProbe.end = std::chrono::steady_clock::now();
}

std::optional<RenderTimeSpan> GLRenderTimeQuery::query()
{
    Q_ASSERT(m_hasResult);
    if (m_gpuProbe.query) {
        const auto previousContext = OpenGlContext::currentContext();
        const auto context = m_context.lock();
        if (!context || !context->makeCurrent()) {
            return std::nullopt;
        }
        GLint64 end = 0;
        glGetQueryObjecti64v(m_gpuProbe.query, GL_QUERY_RESULT, &end);
        m_gpuProbe.end = std::chrono::nanoseconds(end);
        if (previousContext && previousContext != context.get()) {
            previousContext->makeCurrent();
        }
    }

    // timings are pretty unpredictable in the sub-millisecond range; this minimum
    // ensures that when CPU or GPU power states change, we don't drop any frames
    const std::chrono::nanoseconds minimumTime = std::chrono::milliseconds(2);
    const auto end = std::max({m_cpuProbe.start + (m_gpuProbe.end - m_gpuProbe.start), m_cpuProbe.end, m_cpuProbe.start + minimumTime});
    return RenderTimeSpan{
        .start = m_cpuProbe.start,
        .end = end,
    };
}
}
