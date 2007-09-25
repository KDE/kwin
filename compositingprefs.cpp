/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "compositingprefs.h"

#include "options.h"
#include "utils.h"

#include <kdebug.h>


namespace KWin
{

CompositingPrefs::CompositingPrefs()
    {
    mEnableCompositing = false;
    mEnableVSync = true;
    mEnableDirectRendering = true;
    }
CompositingPrefs::~CompositingPrefs()
    {
    }

void CompositingPrefs::detect()
    {
#ifdef HAVE_OPENGL
    // remember and later restore active context
    GLXContext oldcontext = glXGetCurrentContext();
    GLXDrawable olddrawable = glXGetCurrentDrawable();
    GLXDrawable oldreaddrawable = glXGetCurrentReadDrawable();
    if( createGLXContext() )
        {
        detectDriverAndVersion();
        applyDriverSpecificOptions();

        deleteGLXContext();
        }
    if( oldcontext != NULL )
        glXMakeContextCurrent( display(), olddrawable, oldreaddrawable, oldcontext );
#endif
    }

bool CompositingPrefs::createGLXContext()
{
#ifdef HAVE_OPENGL
    // Most of this code has been taken from glxinfo.c
    QVector<int> attribs;
    attribs << GLX_RGBA;
    attribs << GLX_RED_SIZE << 1;
    attribs << GLX_GREEN_SIZE << 1;
    attribs << GLX_BLUE_SIZE << 1;
    attribs << None;

    int scrnum = 0;  // correct?
    XVisualInfo* visinfo = glXChooseVisual( display(), scrnum, attribs.data() );
    if( !visinfo )
        {
        attribs.last() = GLX_DOUBLEBUFFER;
        attribs << None;
        visinfo = glXChooseVisual( display(), scrnum, attribs.data() );
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

    return glXMakeCurrent( display(), mGLWindow, mGLContext );
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
    QString glvendor = QString((const char*)glGetString( GL_VENDOR ));
    QString glrenderer = QString((const char*)glGetString( GL_RENDERER ));
    QString glversion = QString((const char*)glGetString( GL_VERSION ));
    kDebug() << "GL vendor is" << glvendor;
    kDebug() << "GL renderer is" << glrenderer;
    kDebug() << "GL version is" << glversion;

    if( glrenderer.contains( "Intel" ))
    {
        mDriver = "intel";
        QStringList words = glrenderer.split(" ");
        mVersion = Version( words[ words.count() - 2 ] );
    }
    else if( glvendor.contains( "NVIDIA" ))
    {
        mDriver = "nvidia";
        QStringList words = glversion.split(" ");
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
    if( mDriver == "intel")
        {
        kDebug() << "intel driver, disabling vsync, enabling direct";
        mEnableVSync = false;
        mEnableDirectRendering = true;
        if( mVersion >= Version( "20061017" ))
            {
            kDebug() << "intel >= 20061017, enabling compositing";
            mEnableCompositing = true;
            }
        }
    else if( mDriver == "nvidia" )
        {
        kDebug() << "nvidia driver, disabling vsync";
        mEnableVSync = false;
        if( mVersion >= Version( "96.39" ))
            {
            kDebug() << "nvidia >= 96.39, enabling compositing";
            mEnableCompositing = true;
            }
        }
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

