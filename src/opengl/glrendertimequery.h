/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <chrono>
#include <epoxy/gl.h>

#include "kwin_export.h"

namespace KWin
{

class KWIN_EXPORT GLRenderTimeQuery
{
public:
    explicit GLRenderTimeQuery();
    ~GLRenderTimeQuery();

    void begin();
    void end();

    /**
     * fetches the result of the query. If rendering is not done yet, this will block!
     */
    std::chrono::nanoseconds result();

private:
    bool m_hasResult = false;

    struct
    {
        std::chrono::nanoseconds start = std::chrono::nanoseconds::zero();
        std::chrono::nanoseconds end = std::chrono::nanoseconds::zero();
    } m_cpuProbe;

    struct
    {
        GLuint query = 0;
        GLint64 start = 0;
        GLint64 end = 0;
    } m_gpuProbe;
};

}
