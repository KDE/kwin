/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include <config.h>

#include "trackmouse.h"

#include <kglobal.h>
#include <kstandarddirs.h>

#include <math.h>

#ifdef HAVE_OPENGL
#include <GL/gl.h>
#endif

#include <kdebug.h>

namespace KWin
{

KWIN_EFFECT( TrackMouse, TrackMouseEffect )

const int STARS = 5;
const int DIST = 50;

TrackMouseEffect::TrackMouseEffect()
    : active( false )
    , angle( 0 )
    , texture( NULL )
    {
    }

TrackMouseEffect::~TrackMouseEffect()
    {
    delete texture;
    }

void TrackMouseEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( active )
        angle = ( angle + time / 10 ) % 360;
    effects->prePaintScreen( mask, region, time );
    }

void TrackMouseEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data ); // paint normal screen
    if( !active )
        return;
#ifdef HAVE_OPENGL
    if( texture )
        {
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
        texture->bind();
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        for( int i = 0;
             i < STARS;
             ++i )
            {
            QRect r = starRect( i );
            glEnableClientState( GL_VERTEX_ARRAY );
            int verts[ 4 * 2 ] =
                {
                r.x(), r.y(),
                r.x(), r.y() + r.height(),
                r.x() + r.width(), r.y() + r.height(),
                r.x() + r.width(), r.y()
                };
            glVertexPointer( 2, GL_INT, 0, verts );
            glEnableClientState( GL_TEXTURE_COORD_ARRAY );
            int texcoords[ 4 * 2 ] =
                {
                0, 1,
                0, 0,
                1, 0,
                1, 1
                };
            glTexCoordPointer( 2, GL_INT, 0, texcoords );
            glDrawArrays( GL_QUADS, 0, 4 );
            glDisableClientState( GL_TEXTURE_COORD_ARRAY );
            glDisableClientState( GL_VERTEX_ARRAY );
            }
        texture->unbind();
        glPopAttrib();
        }
#endif
    }

void TrackMouseEffect::postPaintScreen()
    {
    if( active )
        {
        for( int i = 0;
             i < STARS;
             ++i )
            effects->addRepaint( starRect( i ));
        }
    effects->postPaintScreen();
    }

void TrackMouseEffect::cursorMoved( const QPoint&, Qt::MouseButtons buttons )
    {
    if( buttons == ( Qt::LeftButton | Qt::RightButton ))
        {
        if( !active )
            {
            if( texture == NULL )
                loadTexture();
            if( texture == NULL )
                return;
            active = true;
            angle = 0;
            }
        for( int i = 0;
             i < STARS;
             ++i )
            effects->addRepaint( starRect( i ));
        }
    else
        {
        if( active )
            {
            for( int i = 0;
                 i < STARS;
                 ++i )
                effects->addRepaint( starRect( i ));
            active = false;
            }
        }
    }

QRect TrackMouseEffect::starRect( int num ) const
    {
    int a = angle + 360 / STARS * num;
    int x = cursorPos().x() + int( DIST * cos( a * ( 2 * M_PI / 360 )));
    int y = cursorPos().y() + int( DIST * sin( a * ( 2 * M_PI / 360 )));
    return QRect( QPoint( x - textureSize.width() / 2,
        y - textureSize.height() / 2 ), textureSize );
    }

void TrackMouseEffect::loadTexture()
    {
#ifdef HAVE_OPENGL
    QString file = KGlobal::dirs()->findResource( "appdata", "trackmouse.png" );
    if( file.isEmpty())
        return;
    QImage im( file );
    texture = new GLTexture( im );
    textureSize = im.size();
#endif
    }

} // namespace
