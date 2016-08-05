/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#include "mock_gl.h"
#include <epoxy/gl.h>

MockGL *s_gl = nullptr;

static const GLubyte *mock_glGetString(GLenum name)
{
    if (!s_gl) {
        return nullptr;
    }
    switch (name) {
    case GL_VENDOR:
        return (const GLubyte*)s_gl->getString.vendor.constData();
    case GL_RENDERER:
        return (const GLubyte*)s_gl->getString.renderer.constData();
    case GL_VERSION:
        return (const GLubyte*)s_gl->getString.version.constData();
    case GL_EXTENSIONS:
        return (const GLubyte*)s_gl->getString.extensionsString.constData();
    case GL_SHADING_LANGUAGE_VERSION:
        return (const GLubyte*)s_gl->getString.shadingLanguageVersion.constData();
    default:
        return nullptr;
    }
}

static const GLubyte *mock_glGetStringi(GLenum name, GLuint index)
{
    if (!s_gl) {
        return nullptr;
    }
    if (name == GL_EXTENSIONS && index < uint(s_gl->getString.extensions.count())) {
        return (const GLubyte*)s_gl->getString.extensions.at(index).constData();
    }
    return nullptr;
}

static void mock_glGetIntegerv(GLenum pname, GLint *data)
{
    Q_UNUSED(pname)
    Q_UNUSED(data)
    if (pname == GL_NUM_EXTENSIONS) {
        if (data && s_gl) {
            *data = s_gl->getString.extensions.count();
        }
    }
}

PFNGLGETSTRINGPROC epoxy_glGetString = mock_glGetString;
PFNGLGETSTRINGIPROC epoxy_glGetStringi = mock_glGetStringi;
PFNGLGETINTEGERVPROC epoxy_glGetIntegerv = mock_glGetIntegerv;
