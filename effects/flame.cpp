/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "flame.h"

#include <assert.h>

namespace KWin
{

KWIN_EFFECT( Flame, FlameEffect )

void FlameEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( !windows.isEmpty())
        *mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    effects->prePaintScreen(mask, region, time);
    }

void FlameEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    if( windows.contains( w ))
        {
        if( windows[ w ] < 1 )
            {
            windows[ w ] += time / 500.;
            *mask |= PAINT_WINDOW_TRANSFORMED;
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DELETE );
            // Request the window to be divided into cells
            w->requestVertexGrid( qMax( w->height() / 50, 5 ));
            }
        else
            {
            windows.remove( w );
            w->unrefWindow();
            }
        }
    effects->prePaintWindow( w, mask, paint, clip, time );
    }

void FlameEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( windows.contains( w ))
        {
        QVector< Vertex >& vertices = w->vertices();
        QVector< Vertex > new_vertices;
        double ylimit = windows[ w ] * w->height(); // parts above this are already away
        assert( vertices.count() % 4 == 0 );
        for( int i = 0;
             i < vertices.count();
             i += 4 )
            {
            bool is_in = false;
            for( int j = 0;
                 j < 4;
                 ++j )
                if( vertices[ i + j ].pos[ 1 ] >= ylimit )
                    is_in = true;
            if( !is_in )
                continue;
            for( int j = 0;
                 j < 4;
                 ++j )
                {
                Vertex vertex = vertices[ i + j ];
                new_vertices.append( vertex );
                }
            }
        if( new_vertices.isEmpty())
            return; // nothing to paint
        w->vertices() = new_vertices;
        w->markVerticesDirty();
        }
    effects->paintWindow( w, mask, region, data );
    }

void FlameEffect::postPaintWindow( EffectWindow* w )
    {
    if( windows.contains( w ))
        effects->addRepaint( w->geometry()); // workspace, since the window under it will need painting too
    effects->postPaintScreen();
    }

void FlameEffect::windowClosed( EffectWindow* c )
    {
    windows[ c ] = 0;
    c->refWindow();
    }

void FlameEffect::windowDeleted( EffectWindow* c )
    {
    windows.remove( c );
    }

} // namespace
