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

#include "compositingprefs.h"

#include "kwinglobals.h"

#include <kdebug.h>
#include <kxerrorhandler.h>
#include <klocale.h>
#include <kdeversion.h>
#include <kstandarddirs.h>

#include <qprocess.h>


namespace KWin
{

CompositingPrefs::CompositingPrefs()
    : mXgl( false )
    , mRecommendCompositing( false )
    , mEnableVSync( true )
    , mEnableDirectRendering( true )
    , mStrictBinding( true )
    {
    }

CompositingPrefs::~CompositingPrefs()
    {
    }

bool CompositingPrefs::recommendCompositing() const
    {
    return mRecommendCompositing;
    }

bool CompositingPrefs::compositingPossible()
    {
#ifdef KWIN_HAVE_COMPOSITING
    Extensions::init();
    if( !Extensions::compositeAvailable())
        {
        kDebug( 1212 ) << "No composite extension available";
        return false;
        }
    if( !Extensions::damageAvailable())
        {
        kDebug( 1212 ) << "No damage extension available";
        return false;
        }
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( Extensions::glxAvailable())
        return true;
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if( Extensions::renderAvailable() && Extensions::fixesAvailable())
        return true;
#endif
    kDebug( 1212 ) << "No OpenGL or XRender/XFixes support";
    return false;
#else
    return false;
#endif
    }

QString CompositingPrefs::compositingNotPossibleReason()
    {
#ifdef KWIN_HAVE_COMPOSITING
    Extensions::init();
    if( !Extensions::compositeAvailable() || !Extensions::damageAvailable())
        {
        return i18n("Required X extensions (XComposite and XDamage) are not available.");
        }
#if defined( KWIN_HAVE_OPENGL_COMPOSITING ) && !defined( KWIN_HAVE_XRENDER_COMPOSITING )
    if( !Extensions::glxAvailable())
        return i18n( "GLX/OpenGL are not available and only OpenGL support is compiled." );
#elif !defined( KWIN_HAVE_OPENGL_COMPOSITING ) && defined( KWIN_HAVE_XRENDER_COMPOSITING )
    if( !( Extensions::renderAvailable() && Extensions::fixesAvailable()))
        return i18n( "XRender/XFixes extensions are not available and only XRender support"
            " is compiled." );
#else
    if( !( Extensions::glxAvailable()
            || ( Extensions::renderAvailable() && Extensions::fixesAvailable())))
        {
        return i18n( "GLX/OpenGL and XRender/XFixes are not available." );
        }
#endif
    return QString();
#else
    return i18n("Compositing was disabled at compile time.\n"
            "It is likely Xorg development headers were not installed.");
#endif
    }

void CompositingPrefs::detect()
    {
    if( !compositingPossible())
        {
        return;
        }

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    // HACK: This is needed for AIGLX
    if( qstrcmp( qgetenv( "KWIN_DIRECT_GL" ), "1" ) != 0 )
        {
        // Start an external helper program that initializes GLX and returns
        // 0 if we can use direct rendering, and 1 otherwise.
        // The reason we have to use an external program is that after GLX
        // has been initialized, it's too late to set the LIBGL_ALWAYS_INDIRECT
        // environment variable.
        // Direct rendering is preferred, since not all OpenGL extensions are
        // available with indirect rendering.
        const QString opengl_test = KStandardDirs::findExe( "kwin_opengl_test" );
        if ( QProcess::execute( opengl_test ) != 0 )
            setenv( "LIBGL_ALWAYS_INDIRECT", "1", true );
        }
    if( !Extensions::glxAvailable())
        {
        kDebug( 1212 ) << "No GLX available";
        return;
        }
    int glxmajor, glxminor;
    glXQueryVersion( display(), &glxmajor, &glxminor );
    kDebug( 1212 ) << "glx version is " << glxmajor << "." << glxminor;
    bool hasglx13 = ( glxmajor > 1 || ( glxmajor == 1 && glxminor >= 3 ));

    // remember and later restore active context
    GLXContext oldcontext = glXGetCurrentContext();
    GLXDrawable olddrawable = glXGetCurrentDrawable();
    GLXDrawable oldreaddrawable = None;
    if( hasglx13 )
        oldreaddrawable = glXGetCurrentReadDrawable();

    if( initGLXContext() )
        {
        detectDriverAndVersion();
        applyDriverSpecificOptions();
        }
    if( hasglx13 )
        glXMakeContextCurrent( display(), olddrawable, oldreaddrawable, oldcontext );
    else
        glXMakeCurrent( display(), olddrawable, oldcontext );
    deleteGLXContext();
#endif
    }

bool CompositingPrefs::initGLXContext()
{
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    mGLContext = NULL;
    KXErrorHandler handler;
    // Most of this code has been taken from glxinfo.c
    QVector<int> attribs;
    attribs << GLX_RGBA;
    attribs << GLX_RED_SIZE << 1;
    attribs << GLX_GREEN_SIZE << 1;
    attribs << GLX_BLUE_SIZE << 1;
    attribs << None;

    XVisualInfo* visinfo = glXChooseVisual( display(), DefaultScreen( display()), attribs.data() );
    if( !visinfo )
        {
        attribs.last() = GLX_DOUBLEBUFFER;
        attribs << None;
        visinfo = glXChooseVisual( display(), DefaultScreen( display()), attribs.data() );
        if (!visinfo)
            {
            kDebug( 1212 ) << "Error: couldn't find RGB GLX visual";
            return false;
            }
        }

    mGLContext = glXCreateContext( display(), visinfo, NULL, True );
    if ( !mGLContext )
    {
        kDebug( 1212 ) << "glXCreateContext failed";
        XDestroyWindow( display(), mGLWindow );
        return false;
    }

    XSetWindowAttributes attr;
    attr.background_pixel = 0;
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap( display(), rootWindow(), visinfo->visual, AllocNone );
    attr.event_mask = StructureNotifyMask | ExposureMask;
    unsigned long mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
    int width = 100, height = 100;
    mGLWindow = XCreateWindow( display(), rootWindow(), 0, 0, width, height,
                       0, visinfo->depth, InputOutput,
                       visinfo->visual, mask, &attr );

    return glXMakeCurrent( display(), mGLWindow, mGLContext ) && !handler.error( true );
#else
   return false;
#endif
}

void CompositingPrefs::deleteGLXContext()
{
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( mGLContext == NULL )
        return;
    glXDestroyContext( display(), mGLContext );
    XDestroyWindow( display(), mGLWindow );
#endif
}

void CompositingPrefs::detectDriverAndVersion()
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    mGLVendor = QString((const char*)glGetString( GL_VENDOR ));
    mGLRenderer = QString((const char*)glGetString( GL_RENDERER ));
    mGLVersion = QString((const char*)glGetString( GL_VERSION ));
    mXgl = detectXgl();

    if( mGLRenderer.startsWith( "Mesa DRI Intel" ) || mGLRenderer.startsWith( "Mesa DRI Mobile Intel" )) // krazy:exclude=strings
        {
        mDriver = "intel";
        QStringList words = mGLRenderer.split(' ');
        mVersion = Version( words[ words.count() - 2 ] );
        }
    else if( mGLVendor == "NVIDIA Corporation" )
        {
        mDriver = "nvidia";
        QStringList words = mGLVersion.split(' ');
        mVersion = Version( words[ words.count() - 1 ] );
        }
    else if( mGLVendor == "ATI Technologies Inc." )
        {
        mDriver = "fglrx";
        // Ati changed the version string.
        // The GL version is either in the first or second part
        QStringList versionParts = mGLVersion.split(' ');
        if( versionParts.first().count(".") == 2 || versionParts.count() == 1 )
            mVersion = Version( versionParts.first() );
        else
            {
            // version in second part is encapsulated in ()
            mVersion = Version( versionParts[1].mid( 1, versionParts[1].length() -2 ) );
            }
        }
    else if( mGLRenderer.startsWith( "Mesa DRI R" )) // krazy:exclude=strings
        {
        mDriver = "radeon";
        mVersion = Version( mGLRenderer.split(' ')[ 3 ] );
        // Check that the version string is changed, and try the fifth element if it does
        if (!mVersion.startsWith("20"))
            mVersion = Version( mGLRenderer.split(' ')[ 5 ] );
        }
    else if( mGLRenderer == "Software Rasterizer" )
        {
        mDriver = "software";
        QStringList words = mGLVersion.split(' ');
        mVersion = Version( words[ words.count() - 1 ] );
        }
    else
        {
        mDriver = "unknown";
        }

    // Always output as it's VERY useful
    kWarning( 1212 ) << "GL vendor is" << mGLVendor;
    kWarning( 1212 ) << "GL renderer is" << mGLRenderer;
    kWarning( 1212 ) << "GL version is" << mGLVersion;
    if( mXgl )
        kWarning( 1212 ) << "Using XGL";
    kWarning( 1212 ) << "Detected driver" << mDriver << ", version" << mVersion.join(".");
#endif
    }

// See http://techbase.kde.org/Projects/KWin/HW for a list of some cards that are known to work.
void CompositingPrefs::applyDriverSpecificOptions()
    {
    // Always recommend
    mRecommendCompositing = true;

    // Known driver specific options
    if( mXgl )
        {
        mStrictBinding = false;
        }
    else if( mDriver == "intel" )
        {
        mEnableVSync = false;
        }
    else if( mDriver == "nvidia" )
        {
        mStrictBinding = false;
        }
    //else if( mDriver == "fglrx" )
    //    {
    //    }
    //else if( mDriver == "radeon" )
    //    {
    //    }
    //else if( mDriver == "software" )
    //    {
    //    }
    }


bool CompositingPrefs::detectXgl()
    { // Xgl apparently uses only this specific X version
    return VendorRelease(display()) == 70000001;
    }

CompositingPrefs::Version::Version( const QString& str ) :
        QStringList()
    {
    QRegExp numrx( "(\\d+)|(\\D+)" );
    int pos = 0;
    while(( pos = numrx.indexIn( str, pos )) != -1 )
        {
        pos += numrx.matchedLength();

        QString part = numrx.cap();
        if( part == "." )
            continue;

        append( part );
        }
    }

int CompositingPrefs::Version::compare( const Version& v ) const
    {
    for( int i = 0; i < qMin( count(), v.count() ); i++ )
        {
        if( at( i )[ 0 ].isDigit() )
            {
            // This part of version string is numeric - compare numbers
            int num = at( i ).toInt();
            int vnum = v.at( i ).toInt();
            if( num > vnum )
                return 1;
            else if( num < vnum )
                return -1;
            }
        else
            {
            // This part is string
            if( at( i ) > v.at( i ))
                return 1;
            else if( at( i ) < v.at( i ))
                return -1;
            }
        }

    if( count() > v.count() )
        return 1;
    else if( count() < v.count() )
        return -1;
    else
        return 0;
    }

} // namespace

