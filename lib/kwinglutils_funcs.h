/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_GLUTILS_FUNCS_H
#define KWIN_GLUTILS_FUNCS_H

#include <kdemacros.h>

#define KWIN_EXPORT KDE_EXPORT

#ifdef HAVE_OPENGL

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


#define GL_TEXTURE_RECTANGLE_ARB                        0x84F5


typedef char GLchar;

// Function pointers
// finding of OpenGL extensions functions
typedef void (*glXFuncPtr)();
typedef glXFuncPtr (*glXGetProcAddress_func)( const GLubyte* );
extern KWIN_EXPORT glXGetProcAddress_func  glXGetProcAddress;
// glXQueryDrawable (added in GLX 1.3)
typedef void (*glXQueryDrawable_func)( Display* dpy, GLXDrawable drawable,
    int attribute, unsigned int *value );
extern KWIN_EXPORT glXQueryDrawable_func glXQueryDrawable;
// texture_from_pixmap extension functions
typedef void (*glXBindTexImageEXT_func)( Display* dpy, GLXDrawable drawable,
    int buffer, const int* attrib_list );
typedef void (*glXReleaseTexImageEXT_func)( Display* dpy, GLXDrawable drawable, int buffer );
extern KWIN_EXPORT glXReleaseTexImageEXT_func glXReleaseTexImageEXT;
extern KWIN_EXPORT glXBindTexImageEXT_func glXBindTexImageEXT;
// glXCopySubBufferMESA
typedef void (*glXCopySubBuffer_func) ( Display* , GLXDrawable, int, int, int, int );
extern KWIN_EXPORT glXCopySubBuffer_func glXCopySubBuffer;
// video_sync extension functions
typedef void (*glXGetVideoSync_func)( unsigned int *count );
typedef void (*glXWaitVideoSync_func)( int divisor, int remainder, unsigned int *count );
extern KWIN_EXPORT glXGetVideoSync_func glXGetVideoSync;
extern KWIN_EXPORT glXWaitVideoSync_func glXWaitVideoSync;
// glActiveTexture
typedef void (*glActiveTexture_func)(GLenum);
extern KWIN_EXPORT glActiveTexture_func glActiveTexture;
// framebuffer_object extension functions
typedef bool (*glIsRenderbuffer_func)( GLuint renderbuffer );
typedef void (*glBindRenderbuffer_func)( GLenum target, GLuint renderbuffer );
typedef void (*glDeleteRenderbuffers_func)( GLsizei n, const GLuint *renderbuffers );
typedef void (*glGenRenderbuffers_func)( GLsizei n, GLuint *renderbuffers );
typedef void (*glRenderbufferStorage_func)( GLenum target, GLenum internalformat, GLsizei width, GLsizei height );
typedef void (*glGetRenderbufferParameteriv_func)( GLenum target, GLenum pname, GLint *params );
typedef bool (*glIsFramebuffer_func)( GLuint framebuffer );
typedef void (*glBindFramebuffer_func)( GLenum target, GLuint framebuffer );
typedef void (*glDeleteFramebuffers_func)( GLsizei n, const GLuint *framebuffers );
typedef void (*glGenFramebuffers_func)( GLsizei n, GLuint *framebuffers );
typedef void (*glCheckFramebufferStatus_func)( GLenum target );
typedef void (*glFramebufferTexture1D_func)( GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level );
typedef void (*glFramebufferTexture2D_func)( GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level );
typedef void (*glFramebufferTexture3D_func)( GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset );
typedef void (*glFramebufferRenderbuffer_func)( GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer );
typedef void (*glGetFramebufferAttachmentParameteriv_func)( GLenum target, GLenum attachment, GLenum pname, GLint *params );
typedef void (*glGenerateMipmap_func)( GLenum target );
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
// Shader stuff
typedef GLuint (*glCreateShader_func)(GLenum);
typedef GLvoid (*glShaderSource_func)(GLuint, GLsizei, const GLchar**, const GLint*);
typedef GLvoid (*glCompileShader_func)(GLuint);
typedef GLvoid (*glDeleteShader_func)(GLuint);
typedef GLuint (*glCreateProgram_func)();
typedef GLvoid (*glAttachShader_func)(GLuint, GLuint);
typedef GLvoid (*glLinkProgram_func)(GLuint);
typedef GLvoid (*glUseProgram_func)(GLuint);
typedef GLvoid (*glDeleteProgram_func)(GLuint);
typedef GLvoid (*glGetShaderInfoLog_func)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLvoid (*glGetProgramInfoLog_func)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLvoid (*glGetProgramiv_func)(GLuint, GLenum, GLint*);
typedef GLvoid (*glGetShaderiv_func)(GLuint, GLenum, GLint*);
typedef GLvoid (*glUniform1f_func)(GLint, GLfloat);
typedef GLvoid (*glUniform1i_func)(GLint, GLint);
typedef GLvoid (*glUniform1fv_func)(GLint, GLsizei, const GLfloat*);
typedef GLvoid (*glUniform2fv_func)(GLint, GLsizei, const GLfloat*);
typedef GLvoid (*glUniform3fv_func)(GLint, GLsizei, const GLfloat*);
typedef GLvoid (*glValidateProgram_func)(GLuint);
typedef GLint (*glGetUniformLocation_func)(GLuint, const GLchar*);
typedef GLvoid (*glVertexAttrib1f_func)(GLuint, GLfloat);
typedef GLint (*glGetAttribLocation_func)(GLuint, const GLchar*);
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
extern KWIN_EXPORT glUniform1i_func glUniform1i;
extern KWIN_EXPORT glUniform1fv_func glUniform1fv;
extern KWIN_EXPORT glUniform2fv_func glUniform2fv;
extern KWIN_EXPORT glValidateProgram_func glValidateProgram;
extern KWIN_EXPORT glGetUniformLocation_func glGetUniformLocation;
extern KWIN_EXPORT glVertexAttrib1f_func glVertexAttrib1f;
extern KWIN_EXPORT glGetAttribLocation_func glGetAttribLocation;

} // namespace

#endif

#endif
