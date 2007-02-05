/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_GLUTILS_FUNCS_H
#define KWIN_GLUTILS_FUNCS_H

namespace KWinInternal
{

void glxResolveFunctions();
void glResolveFunctions();


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


// Function pointers
// finding of OpenGL extensions functions
typedef void (*glXFuncPtr)();
typedef glXFuncPtr (*glXGetProcAddress_func)( const GLubyte* );
extern glXGetProcAddress_func glXGetProcAddress;
// glXQueryDrawable (added in GLX 1.3)
typedef void (*glXQueryDrawable_func)( Display* dpy, GLXDrawable drawable,
    int attribute, unsigned int *value );
extern glXQueryDrawable_func glXQueryDrawable;
// texture_from_pixmap extension functions
typedef void (*glXBindTexImageEXT_func)( Display* dpy, GLXDrawable drawable,
    int buffer, const int* attrib_list );
typedef void (*glXReleaseTexImageEXT_func)( Display* dpy, GLXDrawable drawable, int buffer );
extern glXReleaseTexImageEXT_func glXReleaseTexImageEXT;
extern glXBindTexImageEXT_func glXBindTexImageEXT;
// glXCopySubBufferMESA
typedef void (*glXCopySubBuffer_func) ( Display* , GLXDrawable, int, int, int, int );
extern glXCopySubBuffer_func glXCopySubBuffer;
// video_sync extension functions
typedef void (*glXGetVideoSync_func)( unsigned int *count );
typedef void (*glXWaitVideoSync_func)( int divisor, int remainder, unsigned int *count );
extern glXGetVideoSync_func glXGetVideoSync;
extern glXWaitVideoSync_func glXWaitVideoSync;
// glActiveTexture
typedef void (*glActiveTexture_func)(GLenum);
extern glActiveTexture_func glActiveTexture;
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
extern glIsRenderbuffer_func glIsRenderbuffer;
extern glBindRenderbuffer_func glBindRenderbuffer;
extern glDeleteRenderbuffers_func glDeleteRenderbuffers;
extern glGenRenderbuffers_func glGenRenderbuffers;
extern glRenderbufferStorage_func glRenderbufferStorage;
extern glGetRenderbufferParameteriv_func glGetRenderbufferParameteriv;
extern glIsFramebuffer_func glIsFramebuffer;
extern glBindFramebuffer_func glBindFramebuffer;
extern glDeleteFramebuffers_func glDeleteFramebuffers;
extern glGenFramebuffers_func glGenFramebuffers;
extern glCheckFramebufferStatus_func glCheckFramebufferStatus;
extern glFramebufferTexture1D_func glFramebufferTexture1D;
extern glFramebufferTexture2D_func glFramebufferTexture2D;
extern glFramebufferTexture3D_func glFramebufferTexture3D;
extern glFramebufferRenderbuffer_func glFramebufferRenderbuffer;
extern glGetFramebufferAttachmentParameteriv_func glGetFramebufferAttachmentParameteriv;
extern glGenerateMipmap_func glGenerateMipmap;
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
extern glCreateShader_func glCreateShader;
extern glShaderSource_func glShaderSource;
extern glCompileShader_func glCompileShader;
extern glDeleteShader_func glDeleteShader;
extern glCreateProgram_func glCreateProgram;
extern glAttachShader_func glAttachShader;
extern glLinkProgram_func glLinkProgram;
extern glUseProgram_func glUseProgram;
extern glDeleteProgram_func glDeleteProgram;
extern glGetShaderInfoLog_func glGetShaderInfoLog;
extern glGetProgramInfoLog_func glGetProgramInfoLog;
extern glGetProgramiv_func glGetProgramiv;
extern glGetShaderiv_func glGetShaderiv;
extern glUniform1f_func glUniform1f;
extern glUniform1i_func glUniform1i;
extern glUniform1fv_func glUniform1fv;
extern glUniform2fv_func glUniform2fv;
extern glValidateProgram_func glValidateProgram;
extern glGetUniformLocation_func glGetUniformLocation;
extern glVertexAttrib1f_func glVertexAttrib1f;
extern glGetAttribLocation_func glGetAttribLocation;

} // namespace

#endif

