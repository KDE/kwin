/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "glutils.h"

#include <dlfcn.h>


namespace KWinInternal
{
// Variables
// GL version, use MAKE_OPENGL_VERSION() macro for comparing with a specific version
static int glVersion;
// GLX version, use MAKE_GLX_VERSION() macro for comparing with a specific version
static int glXVersion;
// List of all supported GL and GLX extensions
static QStringList glExtensions;
int glTextureUnitsCount;

// Function pointers
glXGetProcAddress_func glXGetProcAddress;
// texture_from_pixmap extension functions
glXReleaseTexImageEXT_func glXReleaseTexImageEXT;
glXBindTexImageEXT_func glXBindTexImageEXT;
// glActiveTexture
glActiveTexture_func glActiveTexture;
// glXCopySubBufferMESA
glXCopySubBuffer_func glXCopySubBuffer;


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
    // handle OpenGL extensions functions
    glXGetProcAddress = (glXGetProcAddress_func) getProcAddress( "glxGetProcAddress" );
    if( glXGetProcAddress == NULL )
        glXGetProcAddress = (glXGetProcAddress_func) getProcAddress( "glxGetProcAddressARB" );
    glXBindTexImageEXT = (glXBindTexImageEXT_func) getProcAddress( "glXBindTexImageEXT" );
    glXReleaseTexImageEXT = (glXReleaseTexImageEXT_func) getProcAddress( "glXReleaseTexImageEXT" );
    glXCopySubBuffer = (glXCopySubBuffer_func) getProcAddress( "glXCopySubBufferMESA" );

    // Get GLX version
    int major, minor;
    glXQueryVersion( display(), &major, &minor );
    glXVersion = MAKE_GLX_VERSION( major, minor, 0 );
    // Get list of supported GLX extensions. Simply add it to the list of OpenGL extensions.
    glExtensions += QString((const char*)glXQueryExtensionsString(
        display(), DefaultScreen( display()))).split(" ");
    }

void initGL()
    {
    // handle OpenGL extensions functions
    glActiveTexture = (glActiveTexture_func) getProcAddress( "glActiveTexture" );
    if( !glActiveTexture )
        glActiveTexture = (glActiveTexture_func) getProcAddress( "glActiveTextureARB" );

    // Get OpenGL version
    QString glversionstring = QString((const char*)glGetString(GL_VERSION));
    QStringList glversioninfo = glversionstring.left(glversionstring.indexOf(' ')).split('.');
    glVersion = MAKE_OPENGL_VERSION(glversioninfo[0].toInt(), glversioninfo[1].toInt(),
                                    glversioninfo.count() > 2 ? glversioninfo[2].toInt() : 0);
    // Get list of supported OpenGL extensions
    glExtensions = QString((const char*)glGetString(GL_EXTENSIONS)).split(" ");
    // Get number of texture units
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &glTextureUnitsCount);
    }

bool hasGLVersion(int major, int minor, int release)
    {
    return glVersion >= MAKE_OPENGL_VERSION(major, minor, release);
    }

bool hasGLXVersion(int major, int minor, int release)
    {
    return glXVersion >= MAKE_GLX_VERSION(major, minor, release);
    }

bool hasGLExtension(const QString& extension)
    {
    return glExtensions.contains(extension);
    }

} // namespace
