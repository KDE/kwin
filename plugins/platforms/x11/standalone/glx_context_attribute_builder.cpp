/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "glx_context_attribute_builder.h"
#include <epoxy/glx.h>

#ifndef GLX_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV
#define GLX_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV 0x20F7
#endif

namespace KWin
{

std::vector<int> GlxContextAttributeBuilder::build() const
{
    std::vector<int> attribs;
    if (isVersionRequested()) {
        attribs.emplace_back(GLX_CONTEXT_MAJOR_VERSION_ARB);
        attribs.emplace_back(majorVersion());
        attribs.emplace_back(GLX_CONTEXT_MINOR_VERSION_ARB);
        attribs.emplace_back(minorVersion());
    }
    if (isRobust()) {
        attribs.emplace_back(GLX_CONTEXT_FLAGS_ARB);
        attribs.emplace_back(GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB);
        attribs.emplace_back(GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB);
        attribs.emplace_back(GLX_LOSE_CONTEXT_ON_RESET_ARB);
        if (isResetOnVideoMemoryPurge()) {
            attribs.emplace_back(GLX_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV);
            attribs.emplace_back(GL_TRUE);
        }
    }
    attribs.emplace_back(0);
    return attribs;
}

}
