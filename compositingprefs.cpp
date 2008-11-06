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


namespace KWin
{

CompositingPrefs::CompositingPrefs()
    : mXgl( false )
    , mEnableCompositing( false )
    , mEnableVSync( true )
    , mEnableDirectRendering( true )
    , mStrictBinding( true )
    {
    }

CompositingPrefs::~CompositingPrefs()
    {
    }

bool CompositingPrefs::enableCompositing() const
    {
    return mEnableCompositing;
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

// This function checks selected compositing setup and returns false if it should not
// be used even if explicitly configured (unless checks are overridden).
// More checks like broken XRender setups etc. should be added here.
bool CompositingPrefs::validateSetup( CompositingType compositingType ) const
    {
    switch( compositingType )
        {
        case NoCompositing:
            return false;
        case OpenGLCompositing:
            if( mDriver == "software" )
                {
                kDebug( 1212 ) << "Software GL renderer detected, forcing compositing off.";
                return false;
                }
            return true; // allow
        case XRenderCompositing:
            return true; // xrender - always allow?
        }
    abort();
    }

void CompositingPrefs::detect()
    {
    if( !compositingPossible())
        {
        return;
        }

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
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
    kDebug( 1212 ) << "GL vendor is" << mGLVendor;
    kDebug( 1212 ) << "GL renderer is" << mGLRenderer;
    kDebug( 1212 ) << "GL version is" << mGLVersion;
    kDebug( 1212 ) << "XGL:" << ( mXgl ? "yes" : "no" );

    if( mGLRenderer.startsWith( "Mesa DRI Intel" ))
        {
        mDriver = "intel";
        QStringList words = mGLRenderer.split(" ");
        mVersion = Version( words[ words.count() - 2 ] );
        }
    else if( mGLVendor == "NVIDIA Corporation" )
        {
        mDriver = "nvidia";
        QStringList words = mGLVersion.split(" ");
        mVersion = Version( words[ words.count() - 1 ] );
        }
    else if( mGLVendor == "ATI Technologies Inc." )
        {
        mDriver = "fglrx";
        mVersion = Version( mGLVersion.split(" ").first());
        }
    else if( mGLRenderer.startsWith( "Mesa DRI" ))
        {
        mDriver = "radeon";
        mVersion = Version( mGLRenderer.split(" ")[ 3 ] );
        }
    else if( mGLRenderer == "Software Rasterizer" )
        {
        mDriver = "software";
        QStringList words = mGLVersion.split(" ");
        mVersion = Version( words[ words.count() - 1 ] );
        }
    else
        {
        mDriver = "unknown";
        }

    kDebug( 1212 ) << "Detected driver" << mDriver << ", version" << mVersion.join(".");
#endif
    }

// See http://techbase.kde.org/Projects/KWin/HW for a list of some cards that are known to work.
void CompositingPrefs::applyDriverSpecificOptions()
    {
    if( mXgl )
        {
        kDebug( 1212 ) << "xgl, enabling";
        mEnableCompositing = true;
        mStrictBinding = false;
        }
    else if( mDriver == "intel" )
        {
        kDebug( 1212 ) << "intel driver, disabling vsync, enabling direct";
        mEnableVSync = false;
        mEnableDirectRendering = true;
        if( mVersion >= Version( "20061017" ))
            { 
            if( mGLRenderer.contains( "Intel(R) 9" ))
                { // Enable compositing by default on 900-series cards
                kDebug( 1212 ) << "intel >= 20061017 and 900-series card, enabling compositing";
                mEnableCompositing = true;
                }
            if( mGLRenderer.contains( "Mesa DRI Intel(R) G" ))
                { // e.g. G43 chipset
                kDebug( 1212 ) << "intel >= 20061017 and Gxx-series card, enabling compositing";
                mEnableCompositing = true;
                }
            }
        }
    else if( mDriver == "nvidia" )
        {
        mStrictBinding = false;
        if( mVersion >= Version( "173.14.12" ))
            {
            kDebug( 1212 ) << "nvidia >= 173.14.12, enabling compositing";
            mEnableCompositing = true;
            }
        }
    else if( mDriver == "radeon" )
        { // radeon r200 only ?
        if( mGLRenderer.startsWith( "Mesa DRI R200" ) && mVersion >= Version( "20060602" ))
            {
            kDebug( 1212 ) << "radeon r200 >= 20060602, enabling compositing";
            mEnableCompositing = true;
            }
        }
    else if( mDriver == "fglrx" )
        { // radeon r200 only ?
        if( mVersion >= Version( "2.1.7412" ))
            {
            kDebug( 1212 ) << "fglrx >= 2.1.7412, enabling compositing";
            mEnableCompositing = true;
            }
        }
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

