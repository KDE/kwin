/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egl_context_attribute_builder.h"
#include <epoxy/egl.h>

namespace KWin
{

std::vector<int> EglOpenGLESContextAttributeBuilder::build() const
{
    std::vector<int> attribs;
    attribs.emplace_back(EGL_CONTEXT_CLIENT_VERSION);
    attribs.emplace_back(majorVersion());
    if (isRobust()) {
        attribs.emplace_back(EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT);
        attribs.emplace_back(EGL_TRUE);
        attribs.emplace_back(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT);
        attribs.emplace_back(EGL_LOSE_CONTEXT_ON_RESET_EXT);
        if (isResetOnVideoMemoryPurge()) {
            attribs.emplace_back(EGL_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV);
            attribs.emplace_back(GL_TRUE);
        }
    }
    if (isHighPriority()) {
        attribs.emplace_back(EGL_CONTEXT_PRIORITY_LEVEL_IMG);
        attribs.emplace_back(EGL_CONTEXT_PRIORITY_HIGH_IMG);
    }
    attribs.emplace_back(EGL_NONE);
    return attribs;
}

}
