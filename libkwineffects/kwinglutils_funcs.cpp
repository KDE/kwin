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

#include <dlfcn.h>


// Resolves given function, using getProcAddress
#ifdef KWIN_HAVE_EGL
#define GL_RESOLVE( function ) \
    if (platformInterface == GlxPlatformInterface) \
        function = (function ## _func)getProcAddress( #function ); \
    else if (platformInterface == EglPlatformInterface) \
        function = (function ## _func)eglGetProcAddress( #function );
// Same as above but tries to use function "backup" if "function" doesn't exist
// Useful when same functionality is also defined in an extension
#define GL_RESOLVE_WITH_EXT( function, backup ) \
    if (platformInterface == GlxPlatformInterface) { \
        function = (function ## _func)getProcAddress( #function ); \
        if ( !function ) \
            function = (function ## _func)getProcAddress( #backup ); \
    } else if (platformInterface == EglPlatformInterface) { \
        function = (function ## _func)eglGetProcAddress( #function ); \
        if ( !function ) \
            function = (function ## _func)eglGetProcAddress( #backup ); \
    }
#else
// same without the switch to egl
#define GL_RESOLVE( function ) function = (function ## _func)getProcAddress( #function );

#define GL_RESOLVE_WITH_EXT( function, backup ) \
    function = (function ## _func)getProcAddress( #function ); \
    if ( !function ) \
        function = (function ## _func)getProcAddress( #backup );
#endif

#define EGL_RESOLVE( function )  function = (function ## _func)eglGetProcAddress( #function );


namespace KWin
{

#ifndef KWIN_HAVE_OPENGLES
// Function pointers
glXGetProcAddress_func glXGetProcAddress;
// GLX 1.3
glXQueryDrawable_func glXQueryDrawable;
// texture_from_pixmap extension functions
glXReleaseTexImageEXT_func glXReleaseTexImageEXT;
glXBindTexImageEXT_func glXBindTexImageEXT;
// glXCopySubBufferMESA
glXCopySubBuffer_func glXCopySubBuffer;
// video_sync extension functions
glXGetVideoSync_func glXGetVideoSync;
glXWaitVideoSync_func glXWaitVideoSync;
glXSwapInterval_func glXSwapInterval;
// GLX_SGIX_fbconfig
glXGetFBConfigAttrib_func glXGetFBConfigAttrib;
glXGetVisualFromFBConfig_func glXGetVisualFromFBConfig;
glXCreateWindow_func glXCreateWindow;
glXDestroyWindow_func glXDestroyWindow;
glXCreatePixmap_func glXCreatePixmap;
glXDestroyPixmap_func glXDestroyPixmap;
glXGetFBConfigs_func glXGetFBConfigs;

// glActiveTexture
glActiveTexture_func glActiveTexture;
glBlendColor_func glBlendColor;
// framebuffer_object extension functions
glIsRenderbuffer_func glIsRenderbuffer;
glBindRenderbuffer_func glBindRenderbuffer;
glDeleteRenderbuffers_func glDeleteRenderbuffers;
glGenRenderbuffers_func glGenRenderbuffers;
glRenderbufferStorage_func glRenderbufferStorage;
glGetRenderbufferParameteriv_func glGetRenderbufferParameteriv;
glIsFramebuffer_func glIsFramebuffer;
glBindFramebuffer_func glBindFramebuffer;
glDeleteFramebuffers_func glDeleteFramebuffers;
glGenFramebuffers_func glGenFramebuffers;
glCheckFramebufferStatus_func glCheckFramebufferStatus;
glFramebufferTexture1D_func glFramebufferTexture1D;
glFramebufferTexture2D_func glFramebufferTexture2D;
glFramebufferTexture3D_func glFramebufferTexture3D;
glFramebufferRenderbuffer_func glFramebufferRenderbuffer;
glGetFramebufferAttachmentParameteriv_func glGetFramebufferAttachmentParameteriv;
glGenerateMipmap_func glGenerateMipmap;
glBlitFramebuffer_func glBlitFramebuffer;
// Shader functions
glCreateShader_func glCreateShader;
glShaderSource_func glShaderSource;
glCompileShader_func glCompileShader;
glDeleteShader_func glDeleteShader;
glCreateProgram_func glCreateProgram;
glAttachShader_func glAttachShader;
glLinkProgram_func glLinkProgram;
glUseProgram_func glUseProgram;
glDeleteProgram_func glDeleteProgram;
glGetShaderInfoLog_func glGetShaderInfoLog;
glGetProgramInfoLog_func glGetProgramInfoLog;
glGetProgramiv_func glGetProgramiv;
glGetShaderiv_func glGetShaderiv;
glUniform1f_func glUniform1f;
glUniform2f_func glUniform2f;
glUniform3f_func glUniform3f;
glUniform4f_func glUniform4f;
glUniform1i_func glUniform1i;
glUniform1fv_func glUniform1fv;
glUniform2fv_func glUniform2fv;
glUniform3fv_func glUniform3fv;
glUniform4fv_func glUniform4fv;
glGetUniformfv_func glGetUniformfv;
glUniformMatrix4fv_func glUniformMatrix4fv;
glValidateProgram_func glValidateProgram;
glGetUniformLocation_func glGetUniformLocation;
glVertexAttrib1f_func glVertexAttrib1f;
glGetAttribLocation_func glGetAttribLocation;
glBindAttribLocation_func glBindAttribLocation;
glGenProgramsARB_func glGenProgramsARB;
glBindProgramARB_func glBindProgramARB;
glProgramStringARB_func glProgramStringARB;
glProgramLocalParameter4fARB_func glProgramLocalParameter4fARB;
glDeleteProgramsARB_func glDeleteProgramsARB;
glGetProgramivARB_func glGetProgramivARB;
// vertex buffer objects
glGenBuffers_func glGenBuffers;
glDeleteBuffers_func glDeleteBuffers;
glBindBuffer_func glBindBuffer;
glBufferData_func glBufferData;
glEnableVertexAttribArray_func glEnableVertexAttribArray;
glDisableVertexAttribArray_func glDisableVertexAttribArray;
glVertexAttribPointer_func glVertexAttribPointer;


static glXFuncPtr getProcAddress(const char* name)
{
    glXFuncPtr ret = NULL;
    if (glXGetProcAddress != NULL)
        ret = glXGetProcAddress((const GLubyte*) name);
    if (ret == NULL)
        ret = (glXFuncPtr) dlsym(RTLD_DEFAULT, name);
    return ret;
}

void glxResolveFunctions()
{
    // handle OpenGL extensions functions
    glXGetProcAddress = (glXGetProcAddress_func) getProcAddress("glXGetProcAddress");
    if (glXGetProcAddress == NULL)
        glXGetProcAddress = (glXGetProcAddress_func) getProcAddress("glXGetProcAddressARB");
    glXQueryDrawable = (glXQueryDrawable_func) getProcAddress("glXQueryDrawable");
    if (hasGLExtension("GLX_EXT_texture_from_pixmap")) {
        glXBindTexImageEXT = (glXBindTexImageEXT_func) getProcAddress("glXBindTexImageEXT");
        glXReleaseTexImageEXT = (glXReleaseTexImageEXT_func) getProcAddress("glXReleaseTexImageEXT");
    } else {
        glXBindTexImageEXT = NULL;
        glXReleaseTexImageEXT = NULL;
    }
    if (hasGLExtension("GLX_MESA_copy_sub_buffer"))
        glXCopySubBuffer = (glXCopySubBuffer_func) getProcAddress("glXCopySubBufferMESA");
    else
        glXCopySubBuffer = NULL;
    if (hasGLExtension("GLX_SGI_video_sync")) {
        glXGetVideoSync = (glXGetVideoSync_func) getProcAddress("glXGetVideoSyncSGI");
        glXWaitVideoSync = (glXWaitVideoSync_func) getProcAddress("glXWaitVideoSyncSGI");
    } else {
        glXGetVideoSync = NULL;
        glXWaitVideoSync = NULL;
    }

    if (hasGLExtension("GLX_SGI_swap_control")) {
        glXSwapInterval = (glXSwapInterval_func) getProcAddress("glXSwapIntervalSGI");
    } else if (hasGLExtension("GLX_EXT_swap_control")) {
        glXSwapInterval = (glXSwapInterval_func) getProcAddress("glXSwapIntervalEXT");
    } else if (hasGLExtension("GLX_MESA_swap_control")) {
        glXSwapInterval = (glXSwapInterval_func) getProcAddress("glXSwapIntervalMESA");
    } else if (hasGLExtension("GLX_OML_sync_control")) {
        glXSwapInterval = (glXSwapInterval_func) getProcAddress("glXSwapIntervalOML");
    } else {
        glXSwapInterval = NULL;
    }

    OpenGLPlatformInterface platformInterface = GlxPlatformInterface;
    GL_RESOLVE_WITH_EXT(glXGetFBConfigAttrib, glXGetFBConfigAttribSGIX);
    GL_RESOLVE_WITH_EXT(glXGetVisualFromFBConfig, glXGetVisualFromFBConfigSGIX);
    GL_RESOLVE(glXGetFBConfigs);
    GL_RESOLVE(glXCreateWindow);
    GL_RESOLVE(glXDestroyWindow);
    GL_RESOLVE(glXCreatePixmap);
    GL_RESOLVE(glXDestroyPixmap);
}
#endif



#ifdef KWIN_HAVE_EGL

// EGL
eglCreateImageKHR_func eglCreateImageKHR;
eglDestroyImageKHR_func eglDestroyImageKHR;
eglPostSubBufferNV_func eglPostSubBufferNV;
// GLES
glEGLImageTargetTexture2DOES_func glEGLImageTargetTexture2DOES;

void eglResolveFunctions()
{
    if (hasGLExtension("EGL_KHR_image") ||
        (hasGLExtension("EGL_KHR_image_base") &&
         hasGLExtension("EGL_KHR_image_pixmap"))) {
        eglCreateImageKHR = (eglCreateImageKHR_func)eglGetProcAddress("eglCreateImageKHR");
        eglDestroyImageKHR = (eglDestroyImageKHR_func)eglGetProcAddress("eglDestroyImageKHR");
    } else {
        eglCreateImageKHR = NULL;
        eglDestroyImageKHR = NULL;
    }

    if (hasGLExtension("EGL_NV_post_sub_buffer")) {
        eglPostSubBufferNV = (eglPostSubBufferNV_func)eglGetProcAddress("eglPostSubBufferNV");
    } else {
        eglPostSubBufferNV = NULL;
    }
}
#endif

void glResolveFunctions(OpenGLPlatformInterface platformInterface)
{
#ifndef KWIN_HAVE_OPENGLES
    if (hasGLExtension("GL_ARB_multitexture")) {
        GL_RESOLVE_WITH_EXT(glActiveTexture, glActiveTextureARB);
        // Get number of texture units
        glGetIntegerv(GL_MAX_TEXTURE_UNITS, &glTextureUnitsCount);
    } else {
        glActiveTexture = NULL;
        glTextureUnitsCount = 0;
    }
    if (hasGLExtension("GL_EXT_framebuffer_object")) {
        GL_RESOLVE_WITH_EXT(glIsRenderbuffer, glIsRenderbufferEXT);
        GL_RESOLVE_WITH_EXT(glBindRenderbuffer, glBindRenderbufferEXT);
        GL_RESOLVE_WITH_EXT(glDeleteRenderbuffers, glDeleteRenderbuffersEXT);
        GL_RESOLVE_WITH_EXT(glGenRenderbuffers, glGenRenderbuffersEXT);

        GL_RESOLVE_WITH_EXT(glRenderbufferStorage, glRenderbufferStorageEXT);

        GL_RESOLVE_WITH_EXT(glGetRenderbufferParameteriv, glGetRenderbufferParameterivEXT);

        GL_RESOLVE_WITH_EXT(glIsFramebuffer, glIsFramebufferEXT);
        GL_RESOLVE_WITH_EXT(glBindFramebuffer, glBindFramebufferEXT);
        GL_RESOLVE_WITH_EXT(glDeleteFramebuffers, glDeleteFramebuffersEXT);
        GL_RESOLVE_WITH_EXT(glGenFramebuffers, glGenFramebuffersEXT);

        GL_RESOLVE_WITH_EXT(glCheckFramebufferStatus, glCheckFramebufferStatusEXT);

        GL_RESOLVE_WITH_EXT(glFramebufferTexture1D, glFramebufferTexture1DEXT);
        GL_RESOLVE_WITH_EXT(glFramebufferTexture2D, glFramebufferTexture2DEXT);
        GL_RESOLVE_WITH_EXT(glFramebufferTexture3D, glFramebufferTexture3DEXT);

        GL_RESOLVE_WITH_EXT(glFramebufferRenderbuffer, glFramebufferRenderbufferEXT);

        GL_RESOLVE_WITH_EXT(glGetFramebufferAttachmentParameteriv, glGetFramebufferAttachmentParameterivEXT);

        GL_RESOLVE_WITH_EXT(glGenerateMipmap, glGenerateMipmapEXT);
    } else {
        glIsRenderbuffer = NULL;
        glBindRenderbuffer = NULL;
        glDeleteRenderbuffers = NULL;
        glGenRenderbuffers = NULL;
        glRenderbufferStorage = NULL;
        glGetRenderbufferParameteriv = NULL;
        glIsFramebuffer = NULL;
        glBindFramebuffer = NULL;
        glDeleteFramebuffers = NULL;
        glGenFramebuffers = NULL;
        glCheckFramebufferStatus = NULL;
        glFramebufferTexture1D = NULL;
        glFramebufferTexture2D = NULL;
        glFramebufferTexture3D = NULL;
        glFramebufferRenderbuffer = NULL;
        glGetFramebufferAttachmentParameteriv = NULL;
        glGenerateMipmap = NULL;
    }

    if (hasGLExtension("GL_ARB_framebuffer_object")) {
        GL_RESOLVE(glBlitFramebuffer);
    } else if (hasGLExtension("GL_EXT_framebuffer_blit")) {
        GL_RESOLVE_WITH_EXT(glBlitFramebuffer, glBlitFramebufferEXT);
    } else {
        glBlitFramebuffer = NULL;
    }

    if (hasGLExtension("GL_ARB_shading_language_100") && hasGLExtension("GL_ARB_fragment_shader")) {
        GL_RESOLVE_WITH_EXT(glCreateShader, glCreateShaderObjectARB);
        GL_RESOLVE_WITH_EXT(glShaderSource, glShaderSourceARB);
        GL_RESOLVE_WITH_EXT(glCompileShader, glCompileShaderARB);
        GL_RESOLVE_WITH_EXT(glDeleteShader, glDeleteObjectARB);
        GL_RESOLVE_WITH_EXT(glCreateProgram, glCreateProgramObjectARB);
        GL_RESOLVE_WITH_EXT(glAttachShader, glAttachObjectARB);
        GL_RESOLVE_WITH_EXT(glLinkProgram, glLinkProgramARB);
        GL_RESOLVE_WITH_EXT(glUseProgram, glUseProgramObjectARB);
        GL_RESOLVE_WITH_EXT(glDeleteProgram, glDeleteObjectARB);
        GL_RESOLVE_WITH_EXT(glGetShaderInfoLog, glGetInfoLogARB);
        GL_RESOLVE_WITH_EXT(glGetProgramInfoLog, glGetInfoLogARB);
        GL_RESOLVE_WITH_EXT(glGetProgramiv, glGetObjectParameterivARB);
        GL_RESOLVE_WITH_EXT(glGetShaderiv, glGetObjectParameterivARB);
        GL_RESOLVE_WITH_EXT(glUniform1f, glUniform1fARB);
        GL_RESOLVE_WITH_EXT(glUniform2f, glUniform2fARB);
        GL_RESOLVE_WITH_EXT(glUniform3f, glUniform3fARB);
        GL_RESOLVE_WITH_EXT(glUniform4f, glUniform4fARB);
        GL_RESOLVE_WITH_EXT(glUniform1i, glUniform1iARB);
        GL_RESOLVE_WITH_EXT(glUniform1fv, glUniform1fvARB);
        GL_RESOLVE_WITH_EXT(glUniform2fv, glUniform2fvARB);
        GL_RESOLVE_WITH_EXT(glUniform3fv, glUniform3fvARB);
        GL_RESOLVE_WITH_EXT(glUniform4fv, glUniform4fvARB);
        GL_RESOLVE_WITH_EXT(glUniformMatrix4fv, glUniformMatrix4fvARB);
        GL_RESOLVE_WITH_EXT(glValidateProgram, glValidateProgramARB);
        GL_RESOLVE_WITH_EXT(glGetUniformLocation, glGetUniformLocationARB);
        GL_RESOLVE_WITH_EXT(glVertexAttrib1f, glVertexAttrib1fARB);
        GL_RESOLVE_WITH_EXT(glGetAttribLocation, glGetAttribLocationARB);
        GL_RESOLVE_WITH_EXT(glBindAttribLocation, glBindAttribLocationARB);
        GL_RESOLVE_WITH_EXT(glGetUniformfv, glGetUniformfvARB);
    }
    if (hasGLExtension("GL_ARB_fragment_program") && hasGLExtension("GL_ARB_vertex_program")) {
        GL_RESOLVE(glProgramStringARB);
        GL_RESOLVE(glBindProgramARB);
        GL_RESOLVE(glDeleteProgramsARB);
        GL_RESOLVE(glGenProgramsARB);
        GL_RESOLVE(glProgramLocalParameter4fARB);
        GL_RESOLVE(glGetProgramivARB);
    }
    GL_RESOLVE_WITH_EXT(glBlendColor, glBlendColorARB);
    if (hasGLExtension("GL_ARB_vertex_buffer_object")) {
        GL_RESOLVE_WITH_EXT(glGenBuffers, glGenBuffersARB);
        GL_RESOLVE_WITH_EXT(glDeleteBuffers, glDeleteBuffersARB);
        GL_RESOLVE_WITH_EXT(glBindBuffer, glBindBufferARB);
        GL_RESOLVE_WITH_EXT(glBufferData, glBufferDataARB);
        GL_RESOLVE_WITH_EXT(glEnableVertexAttribArray, glEnableVertexAttribArrayARB);
        GL_RESOLVE_WITH_EXT(glDisableVertexAttribArray, glDisableVertexAttribArrayARB);
        GL_RESOLVE_WITH_EXT(glVertexAttribPointer, glVertexAttribPointerARB);
    } else {
        glGenBuffers = NULL;
        glDeleteBuffers = NULL;
        glBindBuffer = NULL;
        glBufferData = NULL;
    }
#endif

#ifdef KWIN_HAVE_EGL
    if (platformInterface == EglPlatformInterface) {
        if (hasGLExtension("GL_OES_EGL_image")) {
            glEGLImageTargetTexture2DOES = (glEGLImageTargetTexture2DOES_func)eglGetProcAddress("glEGLImageTargetTexture2DOES");
        } else {
            glEGLImageTargetTexture2DOES = NULL;
        }
    }
#endif
}

} // namespace
