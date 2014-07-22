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
#include <kwinconfig.h>
#include <kwinglobals.h>

#ifdef KWIN_HAVE_EGL
#include <epoxy/egl.h>
#endif

#ifndef KWIN_HAVE_OPENGLES
#include <epoxy/glx.h>
#endif

#include <fixx11h.h>

#include <epoxy/gl.h>

namespace KWin
{

#ifndef KWIN_HAVE_OPENGLES
void KWINGLUTILS_EXPORT glxResolveFunctions();
#endif

#ifdef KWIN_HAVE_EGL
void KWINGLUTILS_EXPORT eglResolveFunctions();
#endif

void KWINGLUTILS_EXPORT glResolveFunctions(OpenGLPlatformInterface platformInterface);

// GLX_MESA_swap_interval
using glXSwapIntervalMESA_func = int (*)(unsigned int interval);
extern KWINGLUTILS_EXPORT glXSwapIntervalMESA_func glXSwapIntervalMESA;

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
