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


#define MAKE_OPENGL_VERSION(major, minor, release)  ( ((major) << 16) | ((minor) << 8) | (release) )
#define MAKE_GLX_VERSION(major, minor, release)  ( ((major) << 16) | ((minor) << 8) | (release) )


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
// glActiveTexture
typedef void (*glActiveTexture_func)(GLenum);
extern glActiveTexture_func glActiveTexture;
// glXCopySubBufferMESA
typedef void (*glXCopySubBuffer_func) ( Display* , GLXDrawable, int, int, int, int );
extern glXCopySubBuffer_func glXCopySubBuffer;
// video_sync extension functions
typedef void (*glXGetVideoSync_func)( unsigned int *count );
typedef void (*glXWaitVideoSync_func)( int divisor, int remainder, unsigned int *count );
extern glXGetVideoSync_func glXGetVideoSync;
extern glXWaitVideoSync_func glXWaitVideoSync;

} // namespace

#endif
