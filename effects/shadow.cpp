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

namespace KWin
{

KWIN_EFFECT( shadow, ShadowEffect )

ShadowEffect::ShadowEffect()
    {
    KConfigGroup conf = effects->effectConfig("Shadow");
    shadowXOffset = conf.readEntry( "XOffset", 5 );
    shadowYOffset = conf.readEntry( "YOffset", 5 );
    shadowOpacity = (float)conf.readEntry( "Opacity", 0.2 );
    shadowFuzzyness = conf.readEntry( "Fuzzyness", 10 );
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
    QRect r2 = r | r.adjusted( shadowXOffset - shadowFuzzyness, shadowYOffset - shadowFuzzyness,
            shadowXOffset + shadowFuzzyness, shadowYOffset + shadowFuzzyness);
    return effects->transformWindowDamage( w, r2 );
    }

void ShadowEffect::drawShadow( EffectWindow* window, int mask, QRegion region, WindowPaintData& data )
    {
    if(( mask & PAINT_WINDOW_TRANSLUCENT ) == 0 )
        return;
    glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    glPushMatrix();
    if( mask & PAINT_WINDOW_TRANSFORMED )
        glTranslatef( data.xTranslate, data.yTranslate, 0 );
    glTranslatef( window->x() + shadowXOffset, window->y() + shadowYOffset, 0 );
    if(( mask & PAINT_WINDOW_TRANSFORMED ) && ( data.xScale != 1 || data.yScale != 1 ))
        glScalef( data.xScale, data.yScale, 1 );

    int w = window->width();
    int h = window->height();
    int fuzzy = shadowFuzzyness;
    const float verts[ 5 * 4 * 2 ] =
        {
        // center piece (100% opacity)
        0 + fuzzy, 0 + fuzzy,   0 + fuzzy, h - fuzzy,   w - fuzzy, h - fuzzy,   w - fuzzy, 0 + fuzzy,
        // left
        0 - fuzzy, 0 - fuzzy,   0 - fuzzy, h + fuzzy,   0 + fuzzy, h - fuzzy,   0 + fuzzy, 0 + fuzzy,
        // top
        0 - fuzzy, 0 - fuzzy,   0 + fuzzy, 0 + fuzzy,   w - fuzzy, 0 + fuzzy,   w + fuzzy, 0 - fuzzy,
        // right piece
        w - fuzzy, 0 + fuzzy,   w - fuzzy, h - fuzzy,   w + fuzzy, h + fuzzy,   w + fuzzy, 0 - fuzzy,
        // bottom
        0 + fuzzy, h - fuzzy,   0 - fuzzy, h + fuzzy,   w + fuzzy, h + fuzzy,   w - fuzzy, h - fuzzy,
        };
    float opacity = shadowOpacity * data.opacity;
    const float colors[ 5 * 4 * 4 ] =
        {
        // center
        0, 0, 0, opacity,   0, 0, 0, opacity,   0, 0, 0, opacity,   0, 0, 0, opacity,
        // left
        0, 0, 0, 0      ,   0, 0, 0, 0      ,   0, 0, 0, opacity,   0, 0, 0, opacity,
        // top
        0, 0, 0, 0      ,   0, 0, 0, opacity,   0, 0, 0, opacity,   0, 0, 0, 0      ,
        // right
        0, 0, 0, opacity,   0, 0, 0, opacity,   0, 0, 0, 0      ,   0, 0, 0, 0      ,
        // bottom
        0, 0, 0, opacity,   0, 0, 0, 0      ,   0, 0, 0, 0      ,   0, 0, 0, opacity,
        };
    renderGLGeometry( mask, region, 5 * 4, verts, 0, colors );

    glPopMatrix();
    glPopAttrib();
    }

} // namespace
