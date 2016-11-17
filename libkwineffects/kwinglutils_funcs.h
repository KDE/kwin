/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#ifndef KWIN_GLUTILS_FUNCS_H
#define KWIN_GLUTILS_FUNCS_H

#include <kwinglutils_export.h>

#include <epoxy/egl.h>

#include <fixx11h.h>

#include <epoxy/gl.h>
#include <functional>

// qopengl.h declares GLdouble as a typedef of float when Qt is built
// with GLES support.  This conflicts with the epoxy/gl_generated.h
// declaration, so we have to prevent the Qt header from being #included.
#define QOPENGL_H

#ifndef QOPENGLF_APIENTRY
#define QOPENGLF_APIENTRY GLAPIENTRY
#endif

#ifndef QOPENGLF_APIENTRYP
#define QOPENGLF_APIENTRYP GLAPIENTRYP
#endif

namespace KWin
{

typedef void (*resolveFuncPtr)();
void KWINGLUTILS_EXPORT glResolveFunctions(std::function<resolveFuncPtr(const char*)> resolveFunction);

// GL_ARB_robustness / GL_EXT_robustness
using glGetGraphicsResetStatus_func = GLenum (*)();
using glReadnPixels_func = void (*)(GLint x, GLint y, GLsizei width, GLsizei height,
                                    GLenum format, GLenum type, GLsizei bufSize, GLvoid *data);
using glGetnUniformfv_func = void (*)(GLuint program, GLint location, GLsizei bufSize, GLfloat *params);

extern KWINGLUTILS_EXPORT glGetGraphicsResetStatus_func glGetGraphicsResetStatus;
extern KWINGLUTILS_EXPORT glReadnPixels_func            glReadnPixels;
extern KWINGLUTILS_EXPORT glGetnUniformfv_func          glGetnUniformfv;

} // namespace

#endif // KWIN_GLUTILS_FUNCS_H
