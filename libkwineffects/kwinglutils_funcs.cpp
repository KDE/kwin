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
// Same as above but tries to use function "symbolName"
// Useful when functionality is defined in an extension with a different name
#define GL_RESOLVE_WITH_EXT( function, symbolName ) \
    if (platformInterface == GlxPlatformInterface) { \
        function = (function ## _func)getProcAddress( #symbolName ); \
    } else if (platformInterface == EglPlatformInterface) { \
        function = (function ## _func)eglGetProcAddress( #symbolName ); \
    }
#else
// same without the switch to egl
#define GL_RESOLVE( function ) function = (function ## _func)getProcAddress( #function );

#define GL_RESOLVE_WITH_EXT( function, symbolName ) \
    function = (function ## _func)getProcAddress( #symbolName );
#endif

#define EGL_RESOLVE( function )  function = (function ## _func)eglGetProcAddress( #function );


namespace KWin
{

static GLenum GetGraphicsResetStatus();
static void ReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                        GLenum type, GLsizei bufSize, GLvoid *data);
static void GetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params);

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
glXSwapIntervalMESA_func glXSwapIntervalMESA;
glXSwapIntervalEXT_func glXSwapIntervalEXT;
glXSwapIntervalSGI_func glXSwapIntervalSGI;

// GLX_ARB_create_context
glXCreateContextAttribsARB_func glXCreateContextAttribsARB;

// glActiveTexture
glActiveTexture_func glActiveTexture;
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
glBufferSubData_func glBufferSubData;
glGetBufferSubData_func glGetBufferSubData;
glEnableVertexAttribArray_func glEnableVertexAttribArray;
glDisableVertexAttribArray_func glDisableVertexAttribArray;
glVertexAttribPointer_func glVertexAttribPointer;
glMapBuffer_func glMapBuffer;
glUnmapBuffer_func glUnmapBuffer;

// GL_ARB_map_buffer_range
glMapBufferRange_func         glMapBufferRange;
glFlushMappedBufferRange_func glFlushMappedBufferRange;

// GL_ARB_vertex_array_object
glBindVertexArray_func    glBindVertexArray;
glDeleteVertexArrays_func glDeleteVertexArrays;
glGenVertexArrays_func    glGenVertexArrays;
glIsVertexArray_func      glIsVertexArray;


// GL_EXT_gpu_shader4
glVertexAttribI1i_func      glVertexAttribI1i;
glVertexAttribI2i_func      glVertexAttribI2i;
glVertexAttribI3i_func      glVertexAttribI3i;
glVertexAttribI4i_func      glVertexAttribI4i;
glVertexAttribI1ui_func     glVertexAttribI1ui;
glVertexAttribI2ui_func     glVertexAttribI2ui;
glVertexAttribI3ui_func     glVertexAttribI3ui;
glVertexAttribI4ui_func     glVertexAttribI4ui;
glVertexAttribI1iv_func     glVertexAttribI1iv;
glVertexAttribI2iv_func     glVertexAttribI2iv;
glVertexAttribI3iv_func     glVertexAttribI3iv;
glVertexAttribI4iv_func     glVertexAttribI4iv;
glVertexAttribI1uiv_func    glVertexAttribI1uiv;
glVertexAttribI2uiv_func    glVertexAttribI2uiv;
glVertexAttribI3uiv_func    glVertexAttribI3uiv;
glVertexAttribI4uiv_func    glVertexAttribI4uiv;
glVertexAttribI4bv_func     glVertexAttribI4bv;
glVertexAttribI4sv_func     glVertexAttribI4sv;
glVertexAttribI4ubv_func    glVertexAttribI4ubv;
glVertexAttribI4usv_func    glVertexAttribI4usv;
glVertexAttribIPointer_func glVertexAttribIPointer;
glGetVertexAttribIiv_func   glGetVertexAttribIiv;
glGetVertexAttribIuiv_func  glGetVertexAttribIuiv;
glGetUniformuiv_func        glGetUniformuiv;
glBindFragDataLocation_func glBindFragDataLocation;
glGetFragDataLocation_func  glGetFragDataLocation;
glUniform1ui_func           glUniform1ui;
glUniform2ui_func           glUniform2ui;
glUniform3ui_func           glUniform3ui;
glUniform4ui_func           glUniform4ui;
glUniform1uiv_func          glUniform1uiv;
glUniform2uiv_func          glUniform2uiv;
glUniform3uiv_func          glUniform3uiv;

// GL_ARB_robustness
glGetGraphicsResetStatus_func glGetGraphicsResetStatus;
glReadnPixels_func            glReadnPixels;
glGetnUniformfv_func          glGetnUniformfv;

// GL_ARB_draw_elements_base_vertex
glDrawElementsBaseVertex_func          glDrawElementsBaseVertex;
glDrawElementsInstancedBaseVertex_func glDrawElementsInstancedBaseVertex;

// GL_ARB_copy_buffer
glCopyBufferSubData_func glCopyBufferSubData;


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

    if (hasGLExtension("GLX_SGI_swap_control"))
        glXSwapIntervalSGI = (glXSwapIntervalSGI_func) getProcAddress("glXSwapIntervalSGI");
    else
        glXSwapIntervalSGI = NULL;
    if (hasGLExtension("GLX_EXT_swap_control"))
        glXSwapIntervalEXT = (glXSwapIntervalEXT_func) getProcAddress("glXSwapIntervalEXT");
    else
        glXSwapIntervalEXT = NULL;
    if (hasGLExtension("GLX_MESA_swap_control"))
        glXSwapIntervalMESA = (glXSwapIntervalMESA_func) getProcAddress("glXSwapIntervalMESA");
    else
        glXSwapIntervalMESA = NULL;

    if (hasGLExtension("GLX_ARB_create_context"))
        glXCreateContextAttribsARB = (glXCreateContextAttribsARB_func) getProcAddress("glXCreateContextAttribsARB");
    else
        glXCreateContextAttribsARB = NULL;
}

#else

// GL_OES_mapbuffer
glMapBuffer_func         glMapBuffer;
glUnmapBuffer_func       glUnmapBuffer;
glGetBufferPointerv_func glGetBufferPointerv;

// GL_OES_texture_3D
glTexImage3DOES_func     glTexImage3D;

// GL_EXT_map_buffer_range
glMapBufferRange_func         glMapBufferRange;
glFlushMappedBufferRange_func glFlushMappedBufferRange;

#endif // KWIN_HAVE_OPENGLES

#ifdef KWIN_HAVE_EGL

// EGL
eglCreateImageKHR_func eglCreateImageKHR;
eglDestroyImageKHR_func eglDestroyImageKHR;
eglPostSubBufferNV_func eglPostSubBufferNV;
// GLES
glEGLImageTargetTexture2DOES_func glEGLImageTargetTexture2DOES;

#ifdef KWIN_HAVE_OPENGLES
// GL_EXT_robustness
glGetGraphicsResetStatus_func glGetGraphicsResetStatus;
glReadnPixels_func            glReadnPixels;
glGetnUniformfv_func          glGetnUniformfv;
#endif

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
    if (hasGLVersion(1, 3)) {
        GL_RESOLVE(glActiveTexture);
        // Get number of texture units
        glGetIntegerv(GL_MAX_TEXTURE_UNITS, &glTextureUnitsCount);
    } else if (hasGLExtension("GL_ARB_multitexture")) {
        GL_RESOLVE_WITH_EXT(glActiveTexture, glActiveTextureARB);
        // Get number of texture units
        glGetIntegerv(GL_MAX_TEXTURE_UNITS, &glTextureUnitsCount);
    } else {
        glActiveTexture = NULL;
        glTextureUnitsCount = 0;
    }

    if (hasGLVersion(3, 0) || hasGLExtension("GL_ARB_framebuffer_object")) {
        // see http://www.opengl.org/registry/specs/ARB/framebuffer_object.txt
        GL_RESOLVE(glIsRenderbuffer);
        GL_RESOLVE(glBindRenderbuffer);
        GL_RESOLVE(glDeleteRenderbuffers);
        GL_RESOLVE(glGenRenderbuffers);

        GL_RESOLVE(glRenderbufferStorage);

        GL_RESOLVE(glGetRenderbufferParameteriv);

        GL_RESOLVE(glIsFramebuffer);
        GL_RESOLVE(glBindFramebuffer);
        GL_RESOLVE(glDeleteFramebuffers);
        GL_RESOLVE(glGenFramebuffers);

        GL_RESOLVE(glCheckFramebufferStatus);

        GL_RESOLVE(glFramebufferTexture1D);
        GL_RESOLVE(glFramebufferTexture2D);
        GL_RESOLVE(glFramebufferTexture3D);

        GL_RESOLVE(glFramebufferRenderbuffer);

        GL_RESOLVE(glGetFramebufferAttachmentParameteriv);

        GL_RESOLVE(glGenerateMipmap);
    } else if (hasGLExtension("GL_EXT_framebuffer_object")) {
        // see http://www.opengl.org/registry/specs/EXT/framebuffer_object.txt
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

    if (hasGLVersion(3, 0) || hasGLExtension("GL_ARB_framebuffer_object")) {
        // see http://www.opengl.org/registry/specs/ARB/framebuffer_object.txt
        GL_RESOLVE(glBlitFramebuffer);
    } else if (hasGLExtension("GL_EXT_framebuffer_blit")) {
        // see http://www.opengl.org/registry/specs/EXT/framebuffer_blit.txt
        GL_RESOLVE_WITH_EXT(glBlitFramebuffer, glBlitFramebufferEXT);
    } else {
        glBlitFramebuffer = NULL;
    }

    if (hasGLVersion(2, 0)) {
        // see http://www.opengl.org/registry/specs/ARB/shader_objects.txt
        GL_RESOLVE(glCreateShader);
        GL_RESOLVE(glShaderSource);
        GL_RESOLVE(glCompileShader);
        GL_RESOLVE(glDeleteShader);
        GL_RESOLVE(glCreateProgram);
        GL_RESOLVE(glAttachShader);
        GL_RESOLVE(glLinkProgram);
        GL_RESOLVE(glUseProgram);
        GL_RESOLVE(glDeleteProgram);
        GL_RESOLVE(glGetShaderInfoLog);
        GL_RESOLVE(glGetProgramInfoLog);
        GL_RESOLVE(glGetProgramiv);
        GL_RESOLVE(glGetShaderiv);
        GL_RESOLVE(glUniform1f);
        GL_RESOLVE(glUniform2f);
        GL_RESOLVE(glUniform3f);
        GL_RESOLVE(glUniform4f);
        GL_RESOLVE(glUniform1i);
        GL_RESOLVE(glUniform1fv);
        GL_RESOLVE(glUniform2fv);
        GL_RESOLVE(glUniform3fv);
        GL_RESOLVE(glUniform4fv);
        GL_RESOLVE(glUniformMatrix4fv);
        GL_RESOLVE(glValidateProgram);
        GL_RESOLVE(glGetUniformLocation);
        GL_RESOLVE(glGetUniformfv);
    } else if (hasGLExtension("GL_ARB_shader_objects")) {
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
        GL_RESOLVE_WITH_EXT(glGetUniformfv, glGetUniformfvARB);
    } else {
        glCreateShader = NULL;
        glShaderSource = NULL;
        glCompileShader = NULL;
        glDeleteShader = NULL;
        glCreateProgram = NULL;
        glAttachShader = NULL;
        glLinkProgram = NULL;
        glUseProgram = NULL;
        glDeleteProgram = NULL;
        glGetShaderInfoLog = NULL;
        glGetProgramInfoLog = NULL;
        glGetProgramiv = NULL;
        glGetShaderiv = NULL;
        glUniform1f = NULL;
        glUniform2f = NULL;
        glUniform3f = NULL;
        glUniform4f = NULL;
        glUniform1i = NULL;
        glUniform1fv = NULL;
        glUniform2fv = NULL;
        glUniform3fv = NULL;
        glUniform4fv = NULL;
        glUniformMatrix4fv = NULL;
        glValidateProgram = NULL;
        glGetUniformLocation = NULL;
        glGetUniformfv = NULL;
    }

    if (hasGLVersion(2, 0)) {
        // see http://www.opengl.org/registry/specs/ARB/vertex_shader.txt
        GL_RESOLVE(glVertexAttrib1f);
        GL_RESOLVE(glBindAttribLocation);
        GL_RESOLVE(glGetAttribLocation);
        GL_RESOLVE(glEnableVertexAttribArray);
        GL_RESOLVE(glDisableVertexAttribArray);
        GL_RESOLVE(glVertexAttribPointer);
    } else if (hasGLExtension("GL_ARB_vertex_shader")) {
        GL_RESOLVE_WITH_EXT(glVertexAttrib1f, glVertexAttrib1fARB);
        GL_RESOLVE_WITH_EXT(glBindAttribLocation, glBindAttribLocationARB);
        GL_RESOLVE_WITH_EXT(glGetAttribLocation, glGetAttribLocationARB);
        GL_RESOLVE_WITH_EXT(glEnableVertexAttribArray, glEnableVertexAttribArrayARB);
        GL_RESOLVE_WITH_EXT(glDisableVertexAttribArray, glDisableVertexAttribArrayARB);
        GL_RESOLVE_WITH_EXT(glVertexAttribPointer, glVertexAttribPointerARB);
    } else {
        glVertexAttrib1f = NULL;
        glBindAttribLocation = NULL;
        glGetAttribLocation = NULL;
        glEnableVertexAttribArray = NULL;
        glDisableVertexAttribArray = NULL;
        glVertexAttribPointer = NULL;
    }

    if (hasGLExtension("GL_ARB_fragment_program") && hasGLExtension("GL_ARB_vertex_program")) {
        // see http://www.opengl.org/registry/specs/ARB/fragment_program.txt
        GL_RESOLVE(glProgramStringARB);
        GL_RESOLVE(glBindProgramARB);
        GL_RESOLVE(glDeleteProgramsARB);
        GL_RESOLVE(glGenProgramsARB);
        GL_RESOLVE(glProgramLocalParameter4fARB);
        GL_RESOLVE(glGetProgramivARB);
    } else {
        glProgramStringARB = NULL;
        glBindProgramARB = NULL;
        glDeleteProgramsARB = NULL;
        glGenProgramsARB = NULL;
        glProgramLocalParameter4fARB = NULL;
        glGetProgramivARB = NULL;
    }

    if (hasGLVersion(1, 5)) {
        // see http://www.opengl.org/registry/specs/ARB/vertex_buffer_object.txt
        GL_RESOLVE(glGenBuffers);
        GL_RESOLVE(glDeleteBuffers);
        GL_RESOLVE(glBindBuffer);
        GL_RESOLVE(glBufferData);
        GL_RESOLVE(glBufferSubData);
        GL_RESOLVE(glMapBuffer);
        GL_RESOLVE(glUnmapBuffer);
    } else if (hasGLExtension("GL_ARB_vertex_buffer_object")) {
        GL_RESOLVE_WITH_EXT(glGenBuffers, glGenBuffersARB);
        GL_RESOLVE_WITH_EXT(glDeleteBuffers, glDeleteBuffersARB);
        GL_RESOLVE_WITH_EXT(glBindBuffer, glBindBufferARB);
        GL_RESOLVE_WITH_EXT(glBufferData, glBufferDataARB);
        GL_RESOLVE_WITH_EXT(glBufferSubData, glBufferSubDataARB);
        GL_RESOLVE_WITH_EXT(glGetBufferSubData, glGetBufferSubDataARB);
        GL_RESOLVE_WITH_EXT(glMapBuffer, glMapBufferARB);
        GL_RESOLVE_WITH_EXT(glUnmapBuffer, glUnmapBufferARB);
    } else {
        glGenBuffers = NULL;
        glDeleteBuffers = NULL;
        glBindBuffer = NULL;
        glBufferData = NULL;
        glBufferSubData = NULL;
        glGetBufferSubData = NULL;
        glMapBuffer = NULL;
        glUnmapBuffer = NULL;
    }

    if (hasGLVersion(3, 0) || hasGLExtension("GL_ARB_vertex_array_object")) {
        // see http://www.opengl.org/registry/specs/ARB/vertex_array_object.txt
        GL_RESOLVE(glBindVertexArray);
        GL_RESOLVE(glDeleteVertexArrays);
        GL_RESOLVE(glGenVertexArrays);
        GL_RESOLVE(glIsVertexArray);
    } else {
        glBindVertexArray    = NULL;
        glDeleteVertexArrays = NULL;
        glGenVertexArrays    = NULL;
        glIsVertexArray      = NULL;
    }

    if (hasGLVersion(3, 0)) {
        GL_RESOLVE(glVertexAttribI1i);
        GL_RESOLVE(glVertexAttribI2i);
        GL_RESOLVE(glVertexAttribI3i);
        GL_RESOLVE(glVertexAttribI4i);
        GL_RESOLVE(glVertexAttribI1ui);
        GL_RESOLVE(glVertexAttribI2ui);
        GL_RESOLVE(glVertexAttribI3ui);
        GL_RESOLVE(glVertexAttribI4ui);
        GL_RESOLVE(glVertexAttribI1iv);
        GL_RESOLVE(glVertexAttribI2iv);
        GL_RESOLVE(glVertexAttribI3iv);
        GL_RESOLVE(glVertexAttribI4iv);
        GL_RESOLVE(glVertexAttribI1uiv);
        GL_RESOLVE(glVertexAttribI2uiv);
        GL_RESOLVE(glVertexAttribI3uiv);
        GL_RESOLVE(glVertexAttribI4uiv);
        GL_RESOLVE(glVertexAttribI4bv);
        GL_RESOLVE(glVertexAttribI4sv);
        GL_RESOLVE(glVertexAttribI4ubv);
        GL_RESOLVE(glVertexAttribI4usv);
        GL_RESOLVE(glVertexAttribIPointer);
        GL_RESOLVE(glGetVertexAttribIiv);
        GL_RESOLVE(glGetVertexAttribIuiv);
        GL_RESOLVE(glGetUniformuiv);
        GL_RESOLVE(glBindFragDataLocation);
        GL_RESOLVE(glGetFragDataLocation);
        GL_RESOLVE(glUniform1ui);
        GL_RESOLVE(glUniform2ui);
        GL_RESOLVE(glUniform3ui);
        GL_RESOLVE(glUniform4ui);
        GL_RESOLVE(glUniform1uiv);
        GL_RESOLVE(glUniform2uiv);
        GL_RESOLVE(glUniform3uiv);
    } else if (hasGLExtension("GL_EXT_gpu_shader4")) {
        // See http://www.opengl.org/registry/specs/EXT/gpu_shader4.txt
        GL_RESOLVE_WITH_EXT(glVertexAttribI1i,      glVertexAttribI1iEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI2i,      glVertexAttribI2iEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI3i,      glVertexAttribI3iEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI4i,      glVertexAttribI4iEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI1ui,     glVertexAttribI1uiEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI2ui,     glVertexAttribI2uiEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI3ui,     glVertexAttribI3uiEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI4ui,     glVertexAttribI4uiEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI1iv,     glVertexAttribI1ivEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI2iv,     glVertexAttribI2ivEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI3iv,     glVertexAttribI3ivEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI4iv,     glVertexAttribI4ivEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI1uiv,    glVertexAttribI1uivEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI2uiv,    glVertexAttribI2uivEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI3uiv,    glVertexAttribI3uivEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI4uiv,    glVertexAttribI4uivEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI4bv,     glVertexAttribI4bvEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI4sv,     glVertexAttribI4svEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI4ubv,    glVertexAttribI4ubvEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribI4usv,    glVertexAttribI4usvEXT);
        GL_RESOLVE_WITH_EXT(glVertexAttribIPointer, glVertexAttribIPointerEXT);
        GL_RESOLVE_WITH_EXT(glGetVertexAttribIiv,   glGetVertexAttribIivEXT);
        GL_RESOLVE_WITH_EXT(glGetVertexAttribIuiv,  glGetVertexAttribIuivEXT);
        GL_RESOLVE_WITH_EXT(glGetUniformuiv,        glGetUniformuivEXT);
        GL_RESOLVE_WITH_EXT(glBindFragDataLocation, glBindFragDataLocationEXT);
        GL_RESOLVE_WITH_EXT(glGetFragDataLocation,  glGetFragDataLocationEXT);
        GL_RESOLVE_WITH_EXT(glUniform1ui,           glUniform1uiEXT);
        GL_RESOLVE_WITH_EXT(glUniform2ui,           glUniform2uiEXT);
        GL_RESOLVE_WITH_EXT(glUniform3ui,           glUniform3uiEXT);
        GL_RESOLVE_WITH_EXT(glUniform4ui,           glUniform4uiEXT);
        GL_RESOLVE_WITH_EXT(glUniform1uiv,          glUniform1uivEXT);
        GL_RESOLVE_WITH_EXT(glUniform2uiv,          glUniform2uivEXT);
        GL_RESOLVE_WITH_EXT(glUniform3uiv,          glUniform3uivEXT);
    } else {
        glVertexAttribI1i      = NULL;
        glVertexAttribI2i      = NULL;
        glVertexAttribI3i      = NULL;
        glVertexAttribI4i      = NULL;
        glVertexAttribI1ui     = NULL;
        glVertexAttribI2ui     = NULL;
        glVertexAttribI3ui     = NULL;
        glVertexAttribI4ui     = NULL;
        glVertexAttribI1iv     = NULL;
        glVertexAttribI2iv     = NULL;
        glVertexAttribI3iv     = NULL;
        glVertexAttribI4iv     = NULL;
        glVertexAttribI1uiv    = NULL;
        glVertexAttribI2uiv    = NULL;
        glVertexAttribI3uiv    = NULL;
        glVertexAttribI4uiv    = NULL;
        glVertexAttribI4bv     = NULL;
        glVertexAttribI4sv     = NULL;
        glVertexAttribI4ubv    = NULL;
        glVertexAttribI4usv    = NULL;
        glVertexAttribIPointer = NULL;
        glGetVertexAttribIiv   = NULL;
        glGetVertexAttribIuiv  = NULL;
        glGetUniformuiv        = NULL;
        glBindFragDataLocation = NULL;
        glGetFragDataLocation  = NULL;
        glUniform1ui           = NULL;
        glUniform2ui           = NULL;
        glUniform3ui           = NULL;
        glUniform4ui           = NULL;
        glUniform1uiv          = NULL;
        glUniform2uiv          = NULL;
        glUniform3uiv          = NULL;
    }

    if (hasGLVersion(3, 0) || hasGLExtension("GL_ARB_map_buffer_range")) {
        // See http://www.opengl.org/registry/specs/ARB/map_buffer_range.txt
        GL_RESOLVE(glMapBufferRange);
        GL_RESOLVE(glFlushMappedBufferRange);
    } else {
        glMapBufferRange         = NULL;
        glFlushMappedBufferRange = NULL;
    }

    if (hasGLExtension("GL_ARB_robustness")) {
        // See http://www.opengl.org/registry/specs/ARB/robustness.txt
        GL_RESOLVE_WITH_EXT(glGetGraphicsResetStatus, glGetGraphicsResetStatusARB);
        GL_RESOLVE_WITH_EXT(glReadnPixels,            glReadnPixelsARB);
        GL_RESOLVE_WITH_EXT(glGetnUniformfv,          glGetnUniformfvARB);
    } else {
        glGetGraphicsResetStatus = KWin::GetGraphicsResetStatus;
        glReadnPixels            = KWin::ReadnPixels;
        glGetnUniformfv          = KWin::GetnUniformfv;
    }

    if (hasGLVersion(3, 2) || hasGLExtension("GL_ARB_draw_elements_base_vertex")) {
        // See http://www.opengl.org/registry/specs/ARB/draw_elements_base_vertex.txt
        GL_RESOLVE(glDrawElementsBaseVertex);
        GL_RESOLVE(glDrawElementsInstancedBaseVertex);
    } else {
        glDrawElementsBaseVertex          = NULL;
        glDrawElementsInstancedBaseVertex = NULL;
    }

    if (hasGLVersion(3, 1) || hasGLExtension("GL_ARB_copy_buffer")) {
        // See http://www.opengl.org/registry/specs/ARB/copy_buffer.txt
        GL_RESOLVE(glCopyBufferSubData);
    } else {
        glCopyBufferSubData = NULL;
    }

#else

    if (hasGLExtension("GL_OES_mapbuffer")) {
        // See http://www.khronos.org/registry/gles/extensions/OES/OES_mapbuffer.txt
        glMapBuffer         = (glMapBuffer_func)         eglGetProcAddress("glMapBufferOES");
        glUnmapBuffer       = (glUnmapBuffer_func)       eglGetProcAddress("glUnmapBufferOES");
        glGetBufferPointerv = (glGetBufferPointerv_func) eglGetProcAddress("glGetBufferPointervOES");
    } else {
        glMapBuffer         = NULL;
        glUnmapBuffer       = NULL;
        glGetBufferPointerv = NULL;
    }

    if (hasGLExtension("GL_OES_texture_3D")) {
        glTexImage3D = (glTexImage3DOES_func)eglGetProcAddress("glTexImage3DOES");
    } else {
        glTexImage3D = NULL;
    }

    if (hasGLExtension("GL_EXT_map_buffer_range")) {
        // See http://www.khronos.org/registry/gles/extensions/EXT/EXT_map_buffer_range.txt
        glMapBufferRange         = (glMapBufferRange_func)         eglGetProcAddress("glMapBufferRangeEXT");
        glFlushMappedBufferRange = (glFlushMappedBufferRange_func) eglGetProcAddress("glFlushMappedBufferRangeEXT");
    } else {
        glMapBufferRange         = NULL;
        glFlushMappedBufferRange = NULL;
    }

    if (hasGLExtension("GL_EXT_robustness")) {
        // See http://www.khronos.org/registry/gles/extensions/EXT/EXT_robustness.txt
        glGetGraphicsResetStatus = (glGetGraphicsResetStatus_func) eglGetProcAddress("glGetGraphicsResetStatusEXT");
        glReadnPixels            = (glReadnPixels_func)            eglGetProcAddress("glReadnPixelsEXT");
        glGetnUniformfv          = (glGetnUniformfv_func)          eglGetProcAddress("glGetnUniformfvEXT");
    } else {
        glGetGraphicsResetStatus = KWin::GetGraphicsResetStatus;
        glReadnPixels            = KWin::ReadnPixels;
        glGetnUniformfv          = KWin::GetnUniformfv;
    }

#endif // KWIN_HAVE_OPENGLES

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
