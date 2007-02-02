/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_GLUTILS_H
#define KWIN_GLUTILS_H


#include "utils.h"

#include <QStringList>

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>
#include <GL/glxext.h>


namespace KWinInternal
{


// Initializes GLX function pointers
void initGLX();
// Initializes OpenGL stuff. This includes resolving function pointers as
//  well as checking for GL version and extensions
//  Note that GL context has to be created by the time this function is called
void initGL();


// Number of supported texture units
extern int glTextureUnitsCount;


bool hasGLVersion(int major, int minor, int release = 0);
bool hasGLXVersion(int major, int minor, int release = 0);
// use for both OpenGL and GLX extensions
bool hasGLExtension(const QString& extension);

inline bool isPowerOfTwo( int x ) { return (( x & ( x - 1 )) == 0 ); }

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

} // namespace

#endif
