/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "shadow.h"

#ifdef HAVE_OPENGL
#include <GL/gl.h>
#endif

namespace KWinInternal
{

ShadowEffect::ShadowEffect()
    : shadowXOffset( 10 )
    , shadowYOffset( 10 )
    {
    }

void ShadowEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    *mask |= Scene::PAINT_WINDOW_TRANSLUCENT;
    *paint |= ( QRegion( w->geometry()) & *paint ).translated( shadowXOffset, shadowYOffset );
    effects->prePaintWindow( w, mask, paint, clip, time );
    }

void ShadowEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    drawShadow( w, mask, region, data.opacity );
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

void ShadowEffect::drawShadow( EffectWindow* w, int mask, QRegion region, double opacity )
    {
    if(( mask & Scene::PAINT_WINDOW_TRANSLUCENT ) == 0 )
        return;
    glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glColor4f( 0, 0, 0, 0.2 * opacity ); // black
    QRect r( w->geometry());
    r.moveBy( shadowXOffset, shadowYOffset );
    glEnableClientState( GL_VERTEX_ARRAY );
    int verts[ 4 * 2 ] =
        {
        r.x(), r.y(),
        r.x(), r.y() + r.height(),
        r.x() + r.width(), r.y() + r.height(),
        r.x() + r.width(), r.y()
        };
    glVertexPointer( 2, GL_INT, 0, verts );
    if( mask & ( Scene::PAINT_WINDOW_TRANSFORMED | Scene::PAINT_SCREEN_TRANSFORMED ))
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
    glPopAttrib();
    }

} // namespace
