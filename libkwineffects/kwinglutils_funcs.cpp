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

#include "kwinglutils.h"
#include "kwinglplatform.h"

#include <dlfcn.h>
#if HAVE_EPOXY_GLX
#include <epoxy/glx.h>
#endif


// Resolves given function, using getProcAddress
#define GL_RESOLVE( function ) \
    if (platformInterface == GlxPlatformInterface) \
        function = (function ## _func)getProcAddress( #function ); \
    else if (platformInterface == EglPlatformInterface) \
        function = (function ## _func)eglGetProcAddress( #function );
// Same as above but tries to use function "symbolName"
// Useful when functionality is defined in an extension with a different name
#define GL_RESOLVE_WITH_EXT( function, symbolName ) \
    if (platformInterface == GlxPlatformInterface) { \
        function = (function ## _func)getProcAddress( #symbolName ); \
    } else if (platformInterface == EglPlatformInterface) { \
        function = (function ## _func)eglGetProcAddress( #symbolName ); \
    }

namespace KWin
{

static GLenum GetGraphicsResetStatus();
static void ReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                        GLenum type, GLsizei bufSize, GLvoid *data);
static void GetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params);

// GL_ARB_robustness / GL_EXT_robustness
glGetGraphicsResetStatus_func glGetGraphicsResetStatus;
glReadnPixels_func            glReadnPixels;
glGetnUniformfv_func          glGetnUniformfv;

typedef void (*glXFuncPtr)();

static glXFuncPtr getProcAddress(const char* name)
{
    glXFuncPtr ret = nullptr;
#if HAVE_EPOXY_GLX
    ret = glXGetProcAddress((const GLubyte*) name);
#endif
    if (ret == nullptr)
        ret = (glXFuncPtr) dlsym(RTLD_DEFAULT, name);
    return ret;
}

void eglResolveFunctions()
{
}

void glResolveFunctions(OpenGLPlatformInterface platformInterface)
{
    const bool haveArbRobustness = hasGLExtension(QByteArrayLiteral("GL_ARB_robustness"));
    const bool haveExtRobustness = hasGLExtension(QByteArrayLiteral("GL_EXT_robustness"));
    bool robustContext = false;
    if (GLPlatform::instance()->isGLES()) {
        if (haveExtRobustness) {
            GLint value = 0;
            glGetIntegerv(GL_CONTEXT_ROBUST_ACCESS_EXT, &value);
            robustContext = (value != 0);
        }
    } else {
        if (haveArbRobustness) {
            if (hasGLVersion(3, 0)) {
                GLint value = 0;
                glGetIntegerv(GL_CONTEXT_FLAGS, &value);
                if (value & GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT_ARB) {
                    robustContext = true;
                }
            } else {
                robustContext = true;
            }
        }
    }
    if (robustContext && haveArbRobustness) {
        // See http://www.opengl.org/registry/specs/ARB/robustness.txt
        GL_RESOLVE_WITH_EXT(glGetGraphicsResetStatus, glGetGraphicsResetStatusARB);
        GL_RESOLVE_WITH_EXT(glReadnPixels,            glReadnPixelsARB);
        GL_RESOLVE_WITH_EXT(glGetnUniformfv,          glGetnUniformfvARB);
    } else if (robustContext && haveExtRobustness) {
        // See http://www.khronos.org/registry/gles/extensions/EXT/EXT_robustness.txt
        glGetGraphicsResetStatus = (glGetGraphicsResetStatus_func) eglGetProcAddress("glGetGraphicsResetStatusEXT");
        glReadnPixels            = (glReadnPixels_func)            eglGetProcAddress("glReadnPixelsEXT");
        glGetnUniformfv          = (glGetnUniformfv_func)          eglGetProcAddress("glGetnUniformfvEXT");
    } else {
        glGetGraphicsResetStatus = KWin::GetGraphicsResetStatus;
        glReadnPixels            = KWin::ReadnPixels;
        glGetnUniformfv          = KWin::GetnUniformfv;
    }
}

static GLenum GetGraphicsResetStatus()
{
    return GL_NO_ERROR;
}

static void ReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                        GLenum type, GLsizei bufSize, GLvoid *data)
{
    Q_UNUSED(bufSize)
    glReadPixels(x, y, width, height, format, type, data);
}

static void GetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params)
{
    Q_UNUSED(bufSize)
    glGetUniformfv(program, location, params);
}

} // namespace
