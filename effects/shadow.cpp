/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "shadow.h"

#include <kwinglutils.h>

namespace KWin
{

ShadowEffect::ShadowEffect()
    : shadowXOffset( 10 )
    , shadowYOffset( 10 )
    {
    }

void ShadowEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    *mask |= PAINT_WINDOW_TRANSLUCENT;
    *paint |= ( QRegion( w->geometry()) & *paint ).translated( shadowXOffset, shadowYOffset );
    effects->prePaintWindow( w, mask, paint, clip, time );
    }

void ShadowEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( !w->isDeleted() )
        drawShadow( w, mask, region, data );
    effects->paintWindow( w, mask, region, data );
    }

void ShadowEffect::postPaintWindow( EffectWindow* w )
    {
    effects->postPaintWindow( w );
    }

QRect ShadowEffect::transformWindowDamage( EffectWindow* w, const QRect& r )
    {
    QRect r2 = r | r.translated( shadowXOffset, shadowYOffset );
    return effects->transformWindowDamage( w, r2 );
    }

void ShadowEffect::drawShadow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if(( mask & PAINT_WINDOW_TRANSLUCENT ) == 0 )
        return;
    glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glColor4f( 0, 0, 0, 0.2 * data.opacity ); // black

    glPushMatrix();
    if( mask & PAINT_WINDOW_TRANSFORMED )
        glTranslatef( data.xTranslate, data.yTranslate, 0 );
    glTranslatef( w->x() + shadowXOffset, w->y() + shadowYOffset, 0 );
    if(( mask & PAINT_WINDOW_TRANSFORMED ) && ( data.xScale != 1 || data.yScale != 1 ))
        glScalef( data.xScale, data.yScale, 1 );

    glEnableClientState( GL_VERTEX_ARRAY );
    int verts[ 4 * 2 ] =
        {
        0, 0,
        0, w->height(),
         w->width(), w->height(),
         w->width(), 0
        };
    glVertexPointer( 2, GL_INT, 0, verts );
    if( mask & ( PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_TRANSFORMED ))
        glDrawArrays( GL_QUADS, 0, 4 );
    else
        { // clip by region
        glEnable( GL_SCISSOR_TEST );
        int dh = displayHeight();
        foreach( QRect r, region.rects())
            {
            // Scissor rect has to be given in OpenGL coords
            glScissor(r.x(), dh - r.y() - r.height(), r.width(), r.height());
            glDrawArrays( GL_QUADS, 0, 4 );
            }
        }
    glDisableClientState( GL_VERTEX_ARRAY );
    glPopMatrix();
    glPopAttrib();
    }

} // namespace
