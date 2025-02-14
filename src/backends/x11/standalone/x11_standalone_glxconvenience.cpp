/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "x11_standalone_glxconvenience.h"

#include <algorithm>
#include <deque>

namespace KWin
{

GLXFBConfig chooseGlxFbConfig(::Display *display, const int attributes[])
{
    int configCount = 0;
    GLXFBConfig *configs = glXChooseFBConfig(display, DefaultScreen(display),
                                             attributes, &configCount);

    struct FBConfig
    {
        GLXFBConfig config;
        int depth;
        int stencil;
    };

    std::deque<FBConfig> candidates;

    for (int i = 0; i < configCount; i++) {
        int depth, stencil;
        glXGetFBConfigAttrib(display, configs[i], GLX_DEPTH_SIZE, &depth);
        glXGetFBConfigAttrib(display, configs[i], GLX_STENCIL_SIZE, &stencil);

        candidates.emplace_back(FBConfig{configs[i], depth, stencil});
    }

    if (configCount > 0) {
        XFree(configs);
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const FBConfig &left, const FBConfig &right) {
        if (left.depth != right.depth) {
            return left.depth < right.depth;
        }

        return left.stencil < right.stencil;
    });

    return candidates.empty() ? nullptr : candidates.front().config;
}

} // namespace KWin
