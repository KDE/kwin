/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "fallapart.h"

#include <assert.h>
#include <math.h>

namespace KWin
{

KWIN_EFFECT( FallApart, FallApartEffect )

void FallApartEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( !windows.isEmpty())
        *mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    effects->prePaintScreen(mask, region, time);
    }

void FallApartEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    if( windows.contains( w ))
        {
        if( windows[ w ] < 1 )
            {
            windows[ w ] += time / 1000.;
            *mask |= PAINT_WINDOW_TRANSFORMED;
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DELETE );
            // Request the window to be divided into cells
            w->requestVertexGrid( 40 );
            }
        else
            {
            windows.remove( w );
            w->unrefWindow();
            }
        }
    effects->prePaintWindow( w, mask, paint, clip, time );
    }

void FallApartEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( windows.contains( w ))
        {
        QVector< Vertex >& vertices = w->vertices();
        assert( vertices.count() % 4 == 0 );
        for( int i = 0;
             i < vertices.count();
             i += 4 )
            {
            // make fragments move in various directions, based on where
            // they are (left pieces generally move to the left, etc.)
            QPointF p1( vertices[ i ].pos[ 0 ], vertices[ i ].pos[ 1 ] );
            double xdiff = 0;
            if( p1.x() < w->width() / 2 )
                xdiff = -( w->width() / 2 - p1.x()) / w->width() * 100;
            if( p1.x() > w->width() / 2 )
                xdiff = ( p1.x() - w->width() / 2 ) / w->width() * 100;
            double ydiff = 0;
            if( p1.y() < w->height() / 2 )
                ydiff = -( w->height() / 2 - p1.y()) / w->height() * 100;
            if( p1.y() > w->height() / 2 )
                ydiff = ( p1.y() - w->height() / 2 ) / w->height() * 100;
            double modif = windows[ w ] * windows[ w ] * 64;
            srandom( i ); // change direction randomly but consistently
            xdiff += ( rand() % 21 - 10 );
            ydiff += ( rand() % 21 - 10 );
            for( int j = 0;
                 j < 4;
                 ++j )
                {
                vertices[ i + j ].pos[ 0 ] += xdiff * modif;
                vertices[ i + j ].pos[ 1 ] += ydiff * modif;
                }
            // also make the fragments rotate around their center
            QPointF center(( vertices[ i ].pos[ 0 ] + vertices[ i + 1 ].pos[ 0 ]
                + vertices[ i + 2 ].pos[ 0 ] + vertices[ i + 3 ].pos[ 0 ] ) / 4,
                ( vertices[ i ].pos[ 1 ] + vertices[ i + 1 ].pos[ 1 ]
                + vertices[ i + 2 ].pos[ 1 ] + vertices[ i + 3 ].pos[ 1 ] ) / 4 );
            double adiff = ( rand() % 720 - 360 ) / 360. * 2 * M_PI; // spin randomly
            for( int j = 0;
                 j < 4;
                 ++j )
                {
                double x = vertices[ i + j ].pos[ 0 ] - center.x();
                double y = vertices[ i + j ].pos[ 1 ] - center.y();
                double angle = atan2( y, x );
                angle += windows[ w ] * adiff;
                double dist = sqrt( x * x + y * y );
                x = dist * cos( angle );
                y = dist * sin( angle );
                vertices[ i + j ].pos[ 0 ] = center.x() + x;
                vertices[ i + j ].pos[ 1 ] = center.y() + y;
                }
            w->markVerticesDirty();
            }
        }
    effects->paintWindow( w, mask, region, data );
    }

void FallApartEffect::postPaintScreen()
    {
    if( !windows.isEmpty())
        effects->addRepaintFull();
    effects->postPaintScreen();
    }

void FallApartEffect::windowClosed( EffectWindow* c )
    {
    windows[ c ] = 0;
    c->refWindow();
    }

void FallApartEffect::windowDeleted( EffectWindow* c )
    {
    windows.remove( c );
    }

} // namespace
