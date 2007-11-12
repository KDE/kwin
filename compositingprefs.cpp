/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "compositingprefs.h"

#include "kwinglobals.h"

#include <kdebug.h>
#include <kxerrorhandler.h>
#include <klocale.h>


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

bool CompositingPrefs::compositingPossible()
    {
#if defined( HAVE_XCOMPOSITE ) && defined( HAVE_XDAMAGE )
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

    return true;
#else
    return false;
#endif
    }

QString CompositingPrefs::compositingNotPossibleReason()
    {
#if defined( HAVE_XCOMPOSITE ) && defined( HAVE_XDAMAGE )
    Extensions::init();
    if( !Extensions::compositeAvailable() || Extensions::damageAvailable())
        {
        return i18n("Required X extensions (XComposite and XDamage) are not available.");
        }

    return QString();
#else
    return i18n("Compositing was disabled at compile time.\n"
            "Probably Xorg development headers were not installed.");
#endif
    }

void CompositingPrefs::detect()
    {
    if( !compositingPossible())
        {
        return;
        }

#ifdef HAVE_OPENGL
    if( !Extensions::glxAvailable())
        {
        kDebug( 1212 ) << "No GLX available";
        return;
        }
    int glxmajor, glxminor;
    glXQueryVersion( display(), &glxmajor, &glxminor );
    kDebug() << "glx version is " << glxmajor << "." << glxminor;
    bool hasglx13 = ( glxmajor > 1 || ( glxmajor == 1 && glxminor >= 3 ));

    // remember and later restore active context
    GLXContext oldcontext = glXGetCurrentContext();
    GLXDrawable olddrawable = glXGetCurrentDrawable();
    GLXDrawable oldreaddrawable;
    if( hasglx13 )
        oldreaddrawable = glXGetCurrentReadDrawable();

    if( createGLXContext() )
        {
        detectDriverAndVersion();
        applyDriverSpecificOptions();

        deleteGLXContext();
        }
    if( hasglx13 )
        {
        glXMakeContextCurrent( display(), olddrawable, oldreaddrawable, oldcontext );
        }
    else
        {
        glXMakeCurrent( display(), olddrawable, oldcontext );
        }
#endif
    }

bool CompositingPrefs::createGLXContext()
{
#ifdef HAVE_OPENGL
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
            kError() << "Error: couldn't find RGB GLX visual";
            return false;
            }
        }

    mGLContext = glXCreateContext( display(), visinfo, NULL, True );
    if ( !mGLContext )
    {
        kError() << "glXCreateContext failed";
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
#ifdef HAVE_OPENGL
    glXDestroyContext( display(), mGLContext );
    XDestroyWindow( display(), mGLWindow );
#endif
}

void CompositingPrefs::detectDriverAndVersion()
    {
#ifdef HAVE_OPENGL
    mGLVendor = QString((const char*)glGetString( GL_VENDOR ));
    mGLRenderer = QString((const char*)glGetString( GL_RENDERER ));
    mGLVersion = QString((const char*)glGetString( GL_VERSION ));
    mXgl = detectXgl();
    kDebug() << "GL vendor is" << mGLVendor;
    kDebug() << "GL renderer is" << mGLRenderer;
    kDebug() << "GL version is" << mGLVersion;
    kDebug() << "XGL:" << ( mXgl ? "yes" : "no" );

    if( mGLRenderer.contains( "Intel" ))
        {
        mDriver = "intel";
        QStringList words = mGLRenderer.split(" ");
        mVersion = Version( words[ words.count() - 2 ] );
        }
    else if( mGLVendor.contains( "NVIDIA" ))
        {
        mDriver = "nvidia";
        QStringList words = mGLVersion.split(" ");
        mVersion = Version( words[ words.count() - 1 ] );
        }
    else
        {
        mDriver = "unknown";
        }

    kDebug() << "Detected driver" << mDriver << ", version" << mVersion.join(".");
#endif
    }

void CompositingPrefs::applyDriverSpecificOptions()
    {
    if( mXgl )
        {
        kDebug() << "xgl, enabling";
        mEnableCompositing = true;
        mStrictBinding = false;
        }
    else if( mDriver == "intel" )
        {
        kDebug() << "intel driver, disabling vsync, enabling direct";
        mEnableVSync = false;
        mEnableDirectRendering = true;
        if( mVersion >= Version( "20061017" ) && mGLRenderer.contains( "945GM" ))
            {
            kDebug() << "intel >= 20061017 and whitelisted card, enabling compositing";
            mEnableCompositing = true;
            }
        }
    else if( mDriver == "nvidia" )
        {
        mStrictBinding = false;
        if( mVersion >= Version( "100.14.23" ))
            {
            kDebug() << "nvidia >= 100.14.23, enabling compositing";
            mEnableCompositing = true;
            }
        else if( mVersion >= Version( "96.39" ))
            {
            kDebug() << "nvidia >= 96.39, enabling compositing, disabling vsync";
            mEnableCompositing = true;
            mEnableVSync = false;
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

