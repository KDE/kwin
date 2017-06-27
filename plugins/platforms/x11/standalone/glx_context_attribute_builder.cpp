/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
