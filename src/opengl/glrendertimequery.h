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

class EglContext;

class KWIN_EXPORT GLRenderTimeQuery : public RenderTimeQuery
{
public:
    explicit GLRenderTimeQuery(const std::shared_ptr<EglContext> &context, GLuint query);
    ~GLRenderTimeQuery();

    void end();

    /**
     * fetches the result of the query. If rendering is not done yet, this will block!
     */
    std::optional<RenderTimeSpan> query() override;

    static std::unique_ptr<GLRenderTimeQuery> begin(const std::shared_ptr<EglContext> &context);
    static std::unique_ptr<GLRenderTimeQuery> begin();

private:
    const std::weak_ptr<EglContext> m_context;
    bool m_hasResult = false;

    std::chrono::steady_clock::time_point m_cpuStart;

    struct
    {
        GLuint query = 0;
        std::chrono::nanoseconds start{0};
        std::chrono::nanoseconds end{0};
    } m_gpuProbe;

    std::optional<RenderTimeSpan> m_result;
};

}
