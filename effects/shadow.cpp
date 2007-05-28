/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "shadow.h"

#include <kwinglutils.h>

#include <kconfiggroup.h>
#include <kconfig.h>

namespace KWin
{

KWIN_EFFECT( shadow, ShadowEffect )

ShadowEffect::ShadowEffect()
    {
    KConfig c( "kwinrc" );
    KConfigGroup conf( &c, "Effect-Shadow" );
    shadowXOffset = conf.readEntry( "XOffset", 5 );
    shadowYOffset = conf.readEntry( "YOffset", 5 );
    shadowOpacity = (float)conf.readEntry( "Opacity", 0.2 );
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
    glColor4f( 0, 0, 0, shadowOpacity * data.opacity ); // black

    glPushMatrix();
    if( mask & PAINT_WINDOW_TRANSFORMED )
        glTranslatef( data.xTranslate, data.yTranslate, 0 );
    glTranslatef( w->x() + shadowXOffset, w->y() + shadowYOffset, 0 );
    if(( mask & PAINT_WINDOW_TRANSFORMED ) && ( data.xScale != 1 || data.yScale != 1 ))
        glScalef( data.xScale, data.yScale, 1 );

    const float verts[ 4 * 2 ] =
        {
        0, 0,
        0, w->height(),
         w->width(), w->height(),
         w->width(), 0
        };
    renderGLGeometry( mask, region, verts, NULL, 4 );

    glPopMatrix();
    glPopAttrib();
    }

} // namespace
