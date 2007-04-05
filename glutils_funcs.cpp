/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "glutils.h"

#include <dlfcn.h>


// Resolves given function, using getProcAddress
#define GL_RESOLVE( function )  function = (function ## _func)getProcAddress( #function );
// Same as above but tries to use function "backup" if "function" doesn't exist
// Useful when same functionality is also defined in an extension
#define GL_RESOLVE_WITH_EXT( function, backup ) \
  function = (function ## _func)getProcAddress( #function ); \
  if( !function ) \
    function = (function ## _func)getProcAddress( #backup );

#ifdef HAVE_OPENGL

namespace KWin
{

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
glUniform1i_func glUniform1i;
glUniform1fv_func glUniform1fv;
glUniform2fv_func glUniform2fv;
glValidateProgram_func glValidateProgram;
glGetUniformLocation_func glGetUniformLocation;
glVertexAttrib1f_func glVertexAttrib1f;
glGetAttribLocation_func glGetAttribLocation;


static glXFuncPtr getProcAddress( const char* name )
    {
    glXFuncPtr ret = NULL;
    if( glXGetProcAddress != NULL )
        ret = glXGetProcAddress( ( const GLubyte* ) name );
    if( ret == NULL )
        ret = ( glXFuncPtr ) dlsym( RTLD_DEFAULT, name );
    return ret;
    }

void glxResolveFunctions()
    {
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

void glResolveFunctions()
    {
    if( hasGLExtension( "GL_ARB_multitexture" ))
        {
        GL_RESOLVE_WITH_EXT( glActiveTexture, glActiveTextureARB );
        // Get number of texture units
        glGetIntegerv(GL_MAX_TEXTURE_UNITS, &glTextureUnitsCount);
        }
    else
        {
        glActiveTexture = NULL;
        glTextureUnitsCount = 0;
        }
    if( hasGLExtension( "GL_EXT_framebuffer_object" ))
        {
        glIsRenderbuffer = (glIsRenderbuffer_func) getProcAddress( "glIsRenderbufferEXT" );
        glBindRenderbuffer = (glBindRenderbuffer_func) getProcAddress( "glBindRenderbufferEXT" );
        glDeleteRenderbuffers = (glDeleteRenderbuffers_func) getProcAddress( "glDeleteRenderbuffersEXT" );
        glGenRenderbuffers = (glGenRenderbuffers_func) getProcAddress( "glGenRenderbuffersEXT" );

        glRenderbufferStorage = (glRenderbufferStorage_func) getProcAddress( "glRenderbufferStorageEXT" );

        glGetRenderbufferParameteriv = (glGetRenderbufferParameteriv_func) getProcAddress( "glGetRenderbufferParameterivEXT" );

        glIsFramebuffer = (glIsFramebuffer_func) getProcAddress( "glIsFramebufferEXT" );
        glBindFramebuffer = (glBindFramebuffer_func) getProcAddress( "glBindFramebufferEXT" );
        glDeleteFramebuffers = (glDeleteFramebuffers_func) getProcAddress( "glDeleteFramebuffersEXT" );
        glGenFramebuffers = (glGenFramebuffers_func) getProcAddress( "glGenFramebuffersEXT" );

        glCheckFramebufferStatus = (glCheckFramebufferStatus_func) getProcAddress( "glCheckFramebufferStatusEXT" );

        glFramebufferTexture1D = (glFramebufferTexture1D_func) getProcAddress( "glFramebufferTexture1DEXT" );
        glFramebufferTexture2D = (glFramebufferTexture2D_func) getProcAddress( "glFramebufferTexture2DEXT" );
        glFramebufferTexture3D = (glFramebufferTexture3D_func) getProcAddress( "glFramebufferTexture3DEXT" );

        glFramebufferRenderbuffer = (glFramebufferRenderbuffer_func) getProcAddress( "glFramebufferRenderbufferEXT" );

        glGetFramebufferAttachmentParameteriv = (glGetFramebufferAttachmentParameteriv_func) getProcAddress( "glGetFramebufferAttachmentParameterivEXT" );

        glGenerateMipmap = (glGenerateMipmap_func) getProcAddress( "glGenerateMipmapEXT" );
        }
    else
        {
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
    if( hasGLExtension( "GL_ARB_shading_language_100" ) && hasGLExtension( "GL_ARB_fragment_shader" ))
        {
        GL_RESOLVE_WITH_EXT( glCreateShader, glCreateShaderObjectARB );
        GL_RESOLVE_WITH_EXT( glShaderSource, glShaderSourceARB );
        GL_RESOLVE_WITH_EXT( glCompileShader, glCompileShaderARB );
        GL_RESOLVE_WITH_EXT( glDeleteShader, glDeleteObjectARB );
        GL_RESOLVE_WITH_EXT( glCreateProgram, glCreateProgramObjectARB );
        GL_RESOLVE_WITH_EXT( glAttachShader, glAttachObjectARB );
        GL_RESOLVE_WITH_EXT( glLinkProgram, glLinkProgramARB );
        GL_RESOLVE_WITH_EXT( glUseProgram, glUseProgramObjectARB );
        GL_RESOLVE_WITH_EXT( glDeleteProgram, glDeleteObjectARB );
        GL_RESOLVE_WITH_EXT( glGetShaderInfoLog, glGetInfoLogARB );
        GL_RESOLVE_WITH_EXT( glGetProgramInfoLog, glGetInfoLogARB );
        GL_RESOLVE_WITH_EXT( glGetProgramiv, glGetObjectParameterivARB );
        GL_RESOLVE_WITH_EXT( glGetShaderiv, glGetObjectParameterivARB );
        GL_RESOLVE_WITH_EXT( glUniform1f, glUniform1fARB );
        GL_RESOLVE_WITH_EXT( glUniform1i, glUniform1iARB );
        GL_RESOLVE_WITH_EXT( glUniform1fv, glUniform1fvARB );
        GL_RESOLVE_WITH_EXT( glUniform2fv, glUniform2fvARB );
        GL_RESOLVE_WITH_EXT( glValidateProgram, glValidateProgramARB );
        GL_RESOLVE_WITH_EXT( glGetUniformLocation, glGetUniformLocationARB );
        GL_RESOLVE_WITH_EXT( glVertexAttrib1f, glVertexAttrib1fARB );
        GL_RESOLVE_WITH_EXT( glGetAttribLocation, glGetAttribLocationARB );
        }
    }

} // namespace

#endif
