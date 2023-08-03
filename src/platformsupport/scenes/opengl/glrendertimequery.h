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
    GLuint m_query = 0;
    bool m_hasResult = false;
    std::chrono::nanoseconds m_cpuStart = std::chrono::nanoseconds::zero();
    std::chrono::nanoseconds m_cpuEnd = std::chrono::nanoseconds::zero();
};

}
