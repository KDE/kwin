/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "glutils.h"

#include <dlfcn.h>


#define MAKE_GL_VERSION(major, minor, release)  ( ((major) << 16) | ((minor) << 8) | (release) )


namespace KWinInternal
{
// Variables
// GL version, use MAKE_GL_VERSION() macro for comparing with a specific version
static int glVersion;
// GLX version, use MAKE_GL_VERSION() macro for comparing with a specific version
static int glXVersion;
// List of all supported GL and GLX extensions
static QStringList glExtensions;
int glTextureUnitsCount;

// Function pointers
glXGetProcAddress_func glXGetProcAddress;
// GLX 1.3
glXQueryDrawable_func glXQueryDrawable;
// texture_from_pixmap extension functions
glXReleaseTexImageEXT_func glXReleaseTexImageEXT;
glXBindTexImageEXT_func glXBindTexImageEXT;
// glActiveTexture
glActiveTexture_func glActiveTexture;
// glXCopySubBufferMESA
glXCopySubBuffer_func glXCopySubBuffer;
// video_sync extension functions
glXGetVideoSync_func glXGetVideoSync;
glXWaitVideoSync_func glXWaitVideoSync;


// Functions
static glXFuncPtr getProcAddress( const char* name )
    {
    glXFuncPtr ret = NULL;
    if( glXGetProcAddress != NULL )
        ret = glXGetProcAddress( ( const GLubyte* ) name );
    if( ret == NULL )
        ret = ( glXFuncPtr ) dlsym( RTLD_DEFAULT, name );
    return ret;
    }

void initGLX()
    {
    // Get GLX version
    int major, minor;
    glXQueryVersion( display(), &major, &minor );
    glXVersion = MAKE_GL_VERSION( major, minor, 0 );
    // Get list of supported GLX extensions. Simply add it to the list of OpenGL extensions.
    glExtensions += QString((const char*)glXQueryExtensionsString(
        display(), DefaultScreen( display()))).split(" ");

    // handle OpenGL extensions functions
    glXGetProcAddress = (glXGetProcAddress_func) getProcAddress( "glXGetProcAddress" );
    if( glXGetProcAddress == NULL )
        glXGetProcAddress = (glXGetProcAddress_func) getProcAddress( "glXGetProcAddressARB" );
    glXQueryDrawable = (glXQueryDrawable_func) getProcAddress( "glXQueryDrawable" );
    if( hasGLExtension( "GLX_EXT_texture_from_pixmap" ))
        {
        glXBindTexImageEXT = (glXBindTexImageEXT_func) getProcAddress( "glXBindTexImageEXT" );
        glXReleaseTexImageEXT = (glXReleaseTexImageEXT_func) getProcAddress( "glXReleaseTexImageEXT" );
        }
    else
        {
        glXBindTexImageEXT = NULL;
        glXReleaseTexImageEXT = NULL;
        }
    if( hasGLExtension( "GLX_MESA_copy_sub_buffer" ))
        glXCopySubBuffer = (glXCopySubBuffer_func) getProcAddress( "glXCopySubBufferMESA" );
    else
        glXCopySubBuffer = NULL;
    if( hasGLExtension( "GLX_SGI_video_sync" ))
        {
        glXGetVideoSync = (glXGetVideoSync_func) getProcAddress( "glXGetVideoSyncSGI" );
        glXWaitVideoSync = (glXWaitVideoSync_func) getProcAddress( "glXWaitVideoSyncSGI" );
        }
    else
        {
        glXGetVideoSync = NULL;
        glXWaitVideoSync = NULL;
        }
    }

void initGL()
    {
    // Get OpenGL version
    QString glversionstring = QString((const char*)glGetString(GL_VERSION));
    QStringList glversioninfo = glversionstring.left(glversionstring.indexOf(' ')).split('.');
    glVersion = MAKE_GL_VERSION(glversioninfo[0].toInt(), glversioninfo[1].toInt(),
                                    glversioninfo.count() > 2 ? glversioninfo[2].toInt() : 0);
    // Get list of supported OpenGL extensions
    glExtensions = QString((const char*)glGetString(GL_EXTENSIONS)).split(" ");

    // handle OpenGL extensions functions
    if( hasGLExtension( "GL_ARB_multitexture" ))
        {
        glActiveTexture = (glActiveTexture_func) getProcAddress( "glActiveTexture" );
        if( !glActiveTexture )
            glActiveTexture = (glActiveTexture_func) getProcAddress( "glActiveTextureARB" );
        // Get number of texture units
        glGetIntegerv(GL_MAX_TEXTURE_UNITS, &glTextureUnitsCount);
        }
    else
        {
        glActiveTexture = NULL;
        glTextureUnitsCount = 0;
        }
    }

bool hasGLVersion(int major, int minor, int release)
    {
    return glVersion >= MAKE_GL_VERSION(major, minor, release);
    }

bool hasGLXVersion(int major, int minor, int release)
    {
    return glXVersion >= MAKE_GL_VERSION(major, minor, release);
    }

bool hasGLExtension(const QString& extension)
    {
    return glExtensions.contains(extension);
    }

} // namespace
