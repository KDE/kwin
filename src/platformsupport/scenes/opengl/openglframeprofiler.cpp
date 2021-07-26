/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "openglframeprofiler.h"
#include "logging.h"

#include "kwinglplatform.h"
#include "kwinglutils.h"

namespace KWin
{

OpenGLFrameProfiler::OpenGLFrameProfiler()
{
    if (GLPlatform::instance()->supports(GLFeature::TimerQuery)) {
        glGenQueries(1, &m_query);
    }
}

OpenGLFrameProfiler::~OpenGLFrameProfiler()
{
    if (m_query) {
        glDeleteQueries(1, &m_query);
    }
}

void OpenGLFrameProfiler::reset()
{
    m_hasResult = false;
}

void OpenGLFrameProfiler::begin()
{
    if (m_query) {
        glGetInteger64v(GL_TIMESTAMP, reinterpret_cast<GLint64 *>(&m_cpuStart));
    } else {
        m_cpuStart = std::chrono::steady_clock::now().time_since_epoch().count();
    }
}

void OpenGLFrameProfiler::end()
{
    if (m_query) {
        glGetInteger64v(GL_TIMESTAMP, reinterpret_cast<GLint64 *>(&m_cpuEnd));
        glQueryCounter(m_query, GL_TIMESTAMP);
    } else {
        m_cpuEnd = std::chrono::steady_clock::now().time_since_epoch().count();
    }

    m_hasResult = true;
}

std::chrono::nanoseconds OpenGLFrameProfiler::result()
{
    if (!m_hasResult) {
        return std::chrono::nanoseconds::zero();
    }

    std::chrono::nanoseconds start(m_cpuStart);
    std::chrono::nanoseconds end(m_cpuEnd);

    if (m_query) {
        GLuint64 gpuEnd = 0;
        glGetQueryObjectui64v(m_query, GL_QUERY_RESULT, &gpuEnd);

        if (gpuEnd) {
            end = std::max(end, std::chrono::nanoseconds(gpuEnd));
        } else {
            qCDebug(KWIN_OPENGL, "Invalid GPU render timestamp (end: %ld)", gpuEnd);
        }
    }

    return end - start;
}

} // namespace KWin
