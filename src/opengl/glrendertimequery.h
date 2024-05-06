/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <chrono>
#include <epoxy/gl.h>

#include "core/renderbackend.h"
#include "kwin_export.h"

namespace KWin
{

class OpenGlContext;

class KWIN_EXPORT GLRenderTimeQuery : public RenderTimeQuery
{
public:
    explicit GLRenderTimeQuery(const std::shared_ptr<OpenGlContext> &context);
    ~GLRenderTimeQuery();

    void begin();
    void end();

    /**
     * fetches the result of the query. If rendering is not done yet, this will block!
     */
    std::optional<RenderTimeSpan> query() override;

private:
    const std::weak_ptr<OpenGlContext> m_context;
    bool m_hasResult = false;

    struct
    {
        std::chrono::steady_clock::time_point start;
        std::chrono::steady_clock::time_point end;
    } m_cpuProbe;

    struct
    {
        GLuint query = 0;
        std::chrono::nanoseconds start{0};
        std::chrono::nanoseconds end{0};
    } m_gpuProbe;
};

}
