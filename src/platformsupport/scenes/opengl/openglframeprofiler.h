/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwin_export.h>

#include <epoxy/gl.h>

#include <array>
#include <chrono>

namespace KWin
{

/**
 * The OpenGLFrameProfiler class provides a convenient way to record frame timings.
 *
 * The OpenGLFrameProfiler measures how long it takes CPU time to render a frame
 * between begin() and end(). If timer queries are supported, the recorder will also
 * measure how long it takes to execute rendering commands on the GPU.
 *
 * All functions (including the constructor and the destructor) in the OpenGLFrameProfiler
 * require a current OpenGL context.
 */
class KWIN_EXPORT OpenGLFrameProfiler
{
public:
    OpenGLFrameProfiler();
    ~OpenGLFrameProfiler();

    void begin();
    void end();
    void reset();

    std::chrono::nanoseconds result();

private:
    // These timestamps (in ns) are used to measure the CPU render time.
    GLuint64 m_cpuStart = 0;
    GLuint64 m_cpuEnd = 0;
    bool m_hasResult = false;

    // This timer query is used to determine when the gpu finishes rendering.
    GLuint m_query = 0;
};

} // namespace KWin
