/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#include "trackmouse.h"

#include <kwinconfig.h>

#include <kglobal.h>
#include <kstandarddirs.h>

#include <math.h>

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
#include <GL/gl.h>
#endif

#include <kdebug.h>

namespace KWin
{

KWIN_EFFECT( trackmouse, TrackMouseEffect )

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

void TrackMouseEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( active )
        angle = ( angle + time / 10 ) % 360;
    effects->prePaintScreen( data, time );
    }

void TrackMouseEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data ); // paint normal screen
    if( !active )
        return;
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
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
            texture->render( mask, region, r );
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

void TrackMouseEffect::mouseChanged( const QPoint&, const QPoint&, Qt::MouseButtons,
    Qt::MouseButtons, Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers )
    {
    if( modifiers == ( Qt::CTRL | Qt::META ))
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
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    QString file = KGlobal::dirs()->findResource( "appdata", "trackmouse.png" );
    if( file.isEmpty())
        return;
    QImage im( file );
    texture = new GLTexture( im );
    textureSize = im.size();
#endif
    }

} // namespace
