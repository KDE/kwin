/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "glrendertimequery.h"
#include "libkwineffects/kwinglplatform.h"

namespace KWin
{

GLRenderTimeQuery::GLRenderTimeQuery()
{
    if (GLPlatform::instance()->supports(GLFeature::TimerQuery)) {
        glGenQueries(1, &m_query);
    }
}

GLRenderTimeQuery::~GLRenderTimeQuery()
{
    if (m_query) {
        glDeleteQueries(1, &m_query);
    }
}

void GLRenderTimeQuery::begin()
{
    if (m_query) {
        GLint64 nanos = 0;
        glGetInteger64v(GL_TIMESTAMP, &nanos);
        m_cpuStart = std::chrono::nanoseconds(nanos);
    } else {
        m_cpuStart = std::chrono::steady_clock::now().time_since_epoch();
    }
}

void GLRenderTimeQuery::end()
{
    if (m_query) {
        glQueryCounter(m_query, GL_TIMESTAMP);
    } else {
        m_cpuEnd = std::chrono::steady_clock::now().time_since_epoch();
    }
    m_hasResult = true;
}

std::chrono::nanoseconds GLRenderTimeQuery::result()
{
    if (!m_hasResult) {
        return std::chrono::nanoseconds::zero();
    }
    m_hasResult = false;
    if (m_query) {
        uint64_t nanos = 0;
        glGetQueryObjectui64v(m_query, GL_QUERY_RESULT, &nanos);
        return std::chrono::nanoseconds(nanos) - m_cpuStart;
    } else {
        return m_cpuEnd - m_cpuStart;
    }
}

}
