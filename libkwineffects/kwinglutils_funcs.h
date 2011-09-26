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

#include <kdemacros.h>

#define KWIN_EXPORT KDE_EXPORT

#ifndef KWIN_HAVE_OPENGLES

#include <GL/gl.h>
#include <GL/glx.h>

#ifndef GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT 0x8D56
#endif

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER GL_FRAMEBUFFER_EXT
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT
#endif

#ifndef GL_FRAMEBUFFER_UNSUPPORTED
#define GL_FRAMEBUFFER_UNSUPPORTED GL_FRAMEBUFFER_UNSUPPORTED_EXT
#endif

#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#endif

#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_EXT
#endif

#include <fixx11h.h>

namespace KWin
{

void KWIN_EXPORT glxResolveFunctions();
void KWIN_EXPORT glResolveFunctions();


// Defines
/*
** GLX_EXT_texture_from_pixmap
*/
#define GLX_BIND_TO_TEXTURE_RGB_EXT        0x20D0
#define GLX_BIND_TO_TEXTURE_RGBA_EXT       0x20D1
#define GLX_BIND_TO_MIPMAP_TEXTURE_EXT     0x20D2
#define GLX_BIND_TO_TEXTURE_TARGETS_EXT    0x20D3
#define GLX_Y_INVERTED_EXT                 0x20D4

#define GLX_TEXTURE_FORMAT_EXT             0x20D5
#define GLX_TEXTURE_TARGET_EXT             0x20D6
#define GLX_MIPMAP_TEXTURE_EXT             0x20D7

#define GLX_TEXTURE_FORMAT_NONE_EXT        0x20D8
#define GLX_TEXTURE_FORMAT_RGB_EXT         0x20D9
#define GLX_TEXTURE_FORMAT_RGBA_EXT        0x20DA

#define GLX_TEXTURE_1D_BIT_EXT             0x00000001
#define GLX_TEXTURE_2D_BIT_EXT             0x00000002
#define GLX_TEXTURE_RECTANGLE_BIT_EXT      0x00000004

#define GLX_TEXTURE_1D_EXT                 0x20DB
#define GLX_TEXTURE_2D_EXT                 0x20DC
#define GLX_TEXTURE_RECTANGLE_EXT          0x20DD

#define GLX_FRONT_LEFT_EXT                 0x20DE

// Shader stuff
#define GL_COMPILE_STATUS                               0x8B81
#define GL_LINK_STATUS                                  0x8B82
#define GL_INFO_LOG_LENGTH                              0x8B84
#define GL_FRAGMENT_SHADER                              0x8B30
#define GL_VERTEX_SHADER                                0x8B31

// FBO
#define GL_FRAMEBUFFER_EXT                     0x8D40
#define GL_RENDERBUFFER_EXT                    0x8D41
#define GL_STENCIL_INDEX1_EXT                  0x8D46
#define GL_STENCIL_INDEX4_EXT                  0x8D47
#define GL_STENCIL_INDEX8_EXT                  0x8D48
#define GL_STENCIL_INDEX16_EXT                 0x8D49
#define GL_RENDERBUFFER_WIDTH_EXT              0x8D42
#define GL_RENDERBUFFER_HEIGHT_EXT             0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT_EXT    0x8D44
#define GL_RENDERBUFFER_RED_SIZE_EXT           0x8D50
#define GL_RENDERBUFFER_GREEN_SIZE_EXT         0x8D51
#define GL_RENDERBUFFER_BLUE_SIZE_EXT          0x8D52
#define GL_RENDERBUFFER_ALPHA_SIZE_EXT         0x8D53
#define GL_RENDERBUFFER_DEPTH_SIZE_EXT         0x8D54
#define GL_RENDERBUFFER_STENCIL_SIZE_EXT       0x8D55
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT            0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT            0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_EXT          0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_EXT  0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_EXT     0x8CD4
#define GL_COLOR_ATTACHMENT0_EXT               0x8CE0
#define GL_COLOR_ATTACHMENT1_EXT               0x8CE1
#define GL_COLOR_ATTACHMENT2_EXT               0x8CE2
#define GL_COLOR_ATTACHMENT3_EXT               0x8CE3
#define GL_COLOR_ATTACHMENT4_EXT               0x8CE4
#define GL_COLOR_ATTACHMENT5_EXT               0x8CE5
#define GL_COLOR_ATTACHMENT6_EXT               0x8CE6
#define GL_COLOR_ATTACHMENT7_EXT               0x8CE7
#define GL_COLOR_ATTACHMENT8_EXT               0x8CE8
#define GL_COLOR_ATTACHMENT9_EXT               0x8CE9
#define GL_COLOR_ATTACHMENT10_EXT              0x8CEA
#define GL_COLOR_ATTACHMENT11_EXT              0x8CEB
#define GL_COLOR_ATTACHMENT12_EXT              0x8CEC
#define GL_COLOR_ATTACHMENT13_EXT              0x8CED
#define GL_COLOR_ATTACHMENT14_EXT              0x8CEE
#define GL_COLOR_ATTACHMENT15_EXT              0x8CEF
#define GL_DEPTH_ATTACHMENT_EXT                0x8D00
#define GL_STENCIL_ATTACHMENT_EXT              0x8D20
#define GL_FRAMEBUFFER_COMPLETE_EXT                          0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT             0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT     0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT             0x8CD9
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT                0x8CDA
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT            0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT            0x8CDC
#define GL_FRAMEBUFFER_UNSUPPORTED_EXT                       0x8CDD
#define GL_FRAMEBUFFER_BINDING_EXT             0x8CA6
#define GL_RENDERBUFFER_BINDING_EXT            0x8CA7
#define GL_MAX_COLOR_ATTACHMENTS_EXT           0x8CDF
#define GL_MAX_RENDERBUFFER_SIZE_EXT           0x84E8
#define GL_INVALID_FRAMEBUFFER_OPERATION_EXT   0x0506


#define GL_TEXTURE_RECTANGLE_ARB                        0x84F5


// GLX typedefs
typedef struct __GLXcontextRec *GLXContext;
/* GLX 1.3 and later */
typedef struct __GLXFBConfigRec *GLXFBConfig;

// GL typedefs
typedef char GLchar;

// Function pointers
// finding of OpenGL extensions functions
typedef void (*glXFuncPtr)();
typedef glXFuncPtr(*glXGetProcAddress_func)(const GLubyte*);
extern KWIN_EXPORT glXGetProcAddress_func  glXGetProcAddress;
// glXQueryDrawable (added in GLX 1.3)
typedef void (*glXQueryDrawable_func)(Display* dpy, GLXDrawable drawable,
                                      int attribute, unsigned int *value);
extern KWIN_EXPORT glXQueryDrawable_func glXQueryDrawable;
// texture_from_pixmap extension functions
typedef void (*glXBindTexImageEXT_func)(Display* dpy, GLXDrawable drawable,
                                        int buffer, const int* attrib_list);
typedef void (*glXReleaseTexImageEXT_func)(Display* dpy, GLXDrawable drawable, int buffer);
extern KWIN_EXPORT glXReleaseTexImageEXT_func glXReleaseTexImageEXT;
extern KWIN_EXPORT glXBindTexImageEXT_func glXBindTexImageEXT;
// glXCopySubBufferMESA
typedef void (*glXCopySubBuffer_func)(Display* , GLXDrawable, int, int, int, int);
extern KWIN_EXPORT glXCopySubBuffer_func glXCopySubBuffer;
// video_sync extension functions
typedef int (*glXGetVideoSync_func)(unsigned int *count);
typedef int (*glXWaitVideoSync_func)(int divisor, int remainder, unsigned int *count);
extern KWIN_EXPORT glXGetVideoSync_func glXGetVideoSync;
extern KWIN_EXPORT glXWaitVideoSync_func glXWaitVideoSync;
// GLX_SGIX_fbconfig and misc GLX 1.3 stuff
typedef int (*glXGetFBConfigAttrib_func)(Display *dpy, GLXFBConfig config,
        int attribute, int *value);
typedef XVisualInfo*(*glXGetVisualFromFBConfig_func)(Display *dpy, GLXFBConfig config);
extern KWIN_EXPORT glXGetFBConfigAttrib_func glXGetFBConfigAttrib;
extern KWIN_EXPORT glXGetVisualFromFBConfig_func glXGetVisualFromFBConfig;
typedef GLXWindow(*glXCreateWindow_func)(Display *dpy, GLXFBConfig config,
        Window win, const int *attribList);
typedef void (*glXDestroyWindow_func)(Display *dpy, GLXWindow window);
typedef GLXPixmap(*glXCreatePixmap_func)(Display *dpy, GLXFBConfig config,
        Pixmap pixmap, const int *attribList);
typedef void (*glXDestroyPixmap_func)(Display *dpy, GLXPixmap pixmap);
typedef GLXFBConfig*(*glXGetFBConfigs_func)(Display *dpy, int screen, int *nelements);
extern KWIN_EXPORT glXCreateWindow_func glXCreateWindow;
extern KWIN_EXPORT glXDestroyWindow_func glXDestroyWindow;
extern KWIN_EXPORT glXCreatePixmap_func glXCreatePixmap;
extern KWIN_EXPORT glXDestroyPixmap_func glXDestroyPixmap;
extern KWIN_EXPORT glXGetFBConfigs_func glXGetFBConfigs;

// glActiveTexture
typedef void (*glActiveTexture_func)(GLenum);
extern KWIN_EXPORT glActiveTexture_func glActiveTexture;
typedef void (*glBlendColor_func)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
extern KWIN_EXPORT glBlendColor_func glBlendColor;
// framebuffer_object extension functions
typedef GLboolean(*glIsRenderbuffer_func)(GLuint renderbuffer);
typedef void (*glBindRenderbuffer_func)(GLenum target, GLuint renderbuffer);
typedef void (*glDeleteRenderbuffers_func)(GLsizei n, const GLuint *renderbuffers);
typedef void (*glGenRenderbuffers_func)(GLsizei n, GLuint *renderbuffers);
typedef void (*glRenderbufferStorage_func)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (*glGetRenderbufferParameteriv_func)(GLenum target, GLenum pname, GLint *params);
typedef GLboolean(*glIsFramebuffer_func)(GLuint framebuffer);
typedef void (*glBindFramebuffer_func)(GLenum target, GLuint framebuffer);
typedef void (*glDeleteFramebuffers_func)(GLsizei n, const GLuint *framebuffers);
typedef void (*glGenFramebuffers_func)(GLsizei n, GLuint *framebuffers);
typedef GLenum(*glCheckFramebufferStatus_func)(GLenum target);
typedef void (*glFramebufferTexture1D_func)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (*glFramebufferTexture2D_func)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (*glFramebufferTexture3D_func)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
typedef void (*glFramebufferRenderbuffer_func)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (*glGetFramebufferAttachmentParameteriv_func)(GLenum target, GLenum attachment, GLenum pname, GLint *params);
typedef void (*glGenerateMipmap_func)(GLenum target);
typedef void (*glBlitFramebuffer_func)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
extern KWIN_EXPORT glIsRenderbuffer_func glIsRenderbuffer;
extern KWIN_EXPORT glBindRenderbuffer_func glBindRenderbuffer;
extern KWIN_EXPORT glDeleteRenderbuffers_func glDeleteRenderbuffers;
extern KWIN_EXPORT glGenRenderbuffers_func glGenRenderbuffers;
extern KWIN_EXPORT glRenderbufferStorage_func glRenderbufferStorage;
extern KWIN_EXPORT glGetRenderbufferParameteriv_func glGetRenderbufferParameteriv;
extern KWIN_EXPORT glIsFramebuffer_func glIsFramebuffer;
extern KWIN_EXPORT glBindFramebuffer_func glBindFramebuffer;
extern KWIN_EXPORT glDeleteFramebuffers_func glDeleteFramebuffers;
extern KWIN_EXPORT glGenFramebuffers_func glGenFramebuffers;
extern KWIN_EXPORT glCheckFramebufferStatus_func glCheckFramebufferStatus;
extern KWIN_EXPORT glFramebufferTexture1D_func glFramebufferTexture1D;
extern KWIN_EXPORT glFramebufferTexture2D_func glFramebufferTexture2D;
extern KWIN_EXPORT glFramebufferTexture3D_func glFramebufferTexture3D;
extern KWIN_EXPORT glFramebufferRenderbuffer_func glFramebufferRenderbuffer;
extern KWIN_EXPORT glGetFramebufferAttachmentParameteriv_func glGetFramebufferAttachmentParameteriv;
extern KWIN_EXPORT glGenerateMipmap_func glGenerateMipmap;
extern KWIN_EXPORT glBlitFramebuffer_func glBlitFramebuffer;
// Shader stuff
typedef GLuint(*glCreateShader_func)(GLenum);
typedef GLvoid(*glShaderSource_func)(GLuint, GLsizei, const GLchar**, const GLint*);
typedef GLvoid(*glCompileShader_func)(GLuint);
typedef GLvoid(*glDeleteShader_func)(GLuint);
typedef GLuint(*glCreateProgram_func)();
typedef GLvoid(*glAttachShader_func)(GLuint, GLuint);
typedef GLvoid(*glLinkProgram_func)(GLuint);
typedef GLvoid(*glUseProgram_func)(GLuint);
typedef GLvoid(*glDeleteProgram_func)(GLuint);
typedef GLvoid(*glGetShaderInfoLog_func)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLvoid(*glGetProgramInfoLog_func)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLvoid(*glGetProgramiv_func)(GLuint, GLenum, GLint*);
typedef GLvoid(*glGetShaderiv_func)(GLuint, GLenum, GLint*);
typedef GLvoid(*glUniform1f_func)(GLint, GLfloat);
typedef GLvoid(*glUniform2f_func)(GLint, GLfloat, GLfloat);
typedef GLvoid(*glUniform3f_func)(GLint, GLfloat, GLfloat, GLfloat);
typedef GLvoid(*glUniform4f_func)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef GLvoid(*glUniformf_func)(GLint, GLfloat);
typedef GLvoid(*glUniform1i_func)(GLint, GLint);
typedef GLvoid(*glUniform1fv_func)(GLint, GLsizei, const GLfloat*);
typedef GLvoid(*glUniform2fv_func)(GLint, GLsizei, const GLfloat*);
typedef GLvoid(*glUniform3fv_func)(GLint, GLsizei, const GLfloat*);
typedef GLvoid(*glUniform4fv_func)(GLint, GLsizei, const GLfloat*);
typedef GLvoid(*glUniformMatrix4fv_func)(GLint, GLsizei, GLboolean, const GLfloat*);
typedef GLvoid(*glGetUniformfv_func)(GLuint, GLint, GLfloat*);
typedef GLvoid(*glValidateProgram_func)(GLuint);
typedef GLint(*glGetUniformLocation_func)(GLuint, const GLchar*);
typedef GLvoid(*glVertexAttrib1f_func)(GLuint, GLfloat);
typedef GLint(*glGetAttribLocation_func)(GLuint, const GLchar*);
typedef GLvoid(*glBindAttribLocation_func)(GLuint, GLuint, const GLchar*);
typedef void (*glGenProgramsARB_func)(GLsizei, GLuint*);
typedef void (*glBindProgramARB_func)(GLenum, GLuint);
typedef void (*glProgramStringARB_func)(GLenum, GLenum, GLsizei, const GLvoid*);
typedef void (*glProgramLocalParameter4fARB_func)(GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (*glDeleteProgramsARB_func)(GLsizei, const GLuint*);
typedef void (*glGetProgramivARB_func)(GLenum, GLenum, GLint*);
extern KWIN_EXPORT glCreateShader_func glCreateShader;
extern KWIN_EXPORT glShaderSource_func glShaderSource;
extern KWIN_EXPORT glCompileShader_func glCompileShader;
extern KWIN_EXPORT glDeleteShader_func glDeleteShader;
extern KWIN_EXPORT glCreateProgram_func glCreateProgram;
extern KWIN_EXPORT glAttachShader_func glAttachShader;
extern KWIN_EXPORT glLinkProgram_func glLinkProgram;
extern KWIN_EXPORT glUseProgram_func glUseProgram;
extern KWIN_EXPORT glDeleteProgram_func glDeleteProgram;
extern KWIN_EXPORT glGetShaderInfoLog_func glGetShaderInfoLog;
extern KWIN_EXPORT glGetProgramInfoLog_func glGetProgramInfoLog;
extern KWIN_EXPORT glGetProgramiv_func glGetProgramiv;
extern KWIN_EXPORT glGetShaderiv_func glGetShaderiv;
extern KWIN_EXPORT glUniform1f_func glUniform1f;
extern KWIN_EXPORT glUniform2f_func glUniform2f;
extern KWIN_EXPORT glUniform3f_func glUniform3f;
extern KWIN_EXPORT glUniform4f_func glUniform4f;
extern KWIN_EXPORT glUniform1i_func glUniform1i;
extern KWIN_EXPORT glUniform1fv_func glUniform1fv;
extern KWIN_EXPORT glUniform2fv_func glUniform2fv;
extern KWIN_EXPORT glUniform3fv_func glUniform3fv;
extern KWIN_EXPORT glUniform4fv_func glUniform4fv;
extern KWIN_EXPORT glGetUniformfv_func glGetUniformfv;
extern KWIN_EXPORT glUniformMatrix4fv_func glUniformMatrix4fv;
extern KWIN_EXPORT glValidateProgram_func glValidateProgram;
extern KWIN_EXPORT glGetUniformLocation_func glGetUniformLocation;
extern KWIN_EXPORT glVertexAttrib1f_func glVertexAttrib1f;
extern KWIN_EXPORT glGetAttribLocation_func glGetAttribLocation;
extern KWIN_EXPORT glBindAttribLocation_func glBindAttribLocation;
extern KWIN_EXPORT glGenProgramsARB_func glGenProgramsARB;
extern KWIN_EXPORT glBindProgramARB_func glBindProgramARB;
extern KWIN_EXPORT glProgramStringARB_func glProgramStringARB;
extern KWIN_EXPORT glProgramLocalParameter4fARB_func glProgramLocalParameter4fARB;
extern KWIN_EXPORT glDeleteProgramsARB_func glDeleteProgramsARB;
extern KWIN_EXPORT glGetProgramivARB_func glGetProgramivARB;
// vertex buffer objects
typedef void (*glGenBuffers_func)(GLsizei, GLuint*);
extern KWIN_EXPORT glGenBuffers_func glGenBuffers;
typedef void (*glDeleteBuffers_func)(GLsizei n, const GLuint*);
extern KWIN_EXPORT glDeleteBuffers_func glDeleteBuffers;
typedef void (*glBindBuffer_func)(GLenum, GLuint);
extern KWIN_EXPORT glBindBuffer_func glBindBuffer;
typedef void (*glBufferData_func)(GLenum, GLsizeiptr, const GLvoid*, GLenum);
extern KWIN_EXPORT glBufferData_func glBufferData;
typedef void (*glEnableVertexAttribArray_func)(GLuint);
extern KWIN_EXPORT glEnableVertexAttribArray_func glEnableVertexAttribArray;
typedef void (*glDisableVertexAttribArray_func)(GLuint);
extern KWIN_EXPORT glDisableVertexAttribArray_func glDisableVertexAttribArray;
typedef void (*glVertexAttribPointer_func)(GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid*);
extern KWIN_EXPORT glVertexAttribPointer_func glVertexAttribPointer;

} // namespace

#else
#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <fixx11h.h>

namespace KWin
{

void KWIN_EXPORT eglResolveFunctions();
void KWIN_EXPORT glResolveFunctions();
// EGL
typedef EGLImageKHR(*eglCreateImageKHR_func)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint*);
typedef EGLBoolean(*eglDestroyImageKHR_func)(EGLDisplay, EGLImageKHR);
extern KWIN_EXPORT eglCreateImageKHR_func eglCreateImageKHR;
extern KWIN_EXPORT eglDestroyImageKHR_func eglDestroyImageKHR;

// GLES
typedef GLvoid(*glEGLImageTargetTexture2DOES_func)(GLenum, GLeglImageOES);
extern KWIN_EXPORT glEGLImageTargetTexture2DOES_func glEGLImageTargetTexture2DOES;

} // namespace

#endif

#endif
