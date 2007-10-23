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
#include <KStandardDirs>

namespace KWin
{

KWIN_EFFECT( shadow, ShadowEffect )

ShadowEffect::ShadowEffect()
    {
    KConfigGroup conf = effects->effectConfig("Shadow");
    shadowXOffset = conf.readEntry( "XOffset", 5 );
    shadowYOffset = conf.readEntry( "YOffset", 5 );
    shadowOpacity = conf.readEntry( "Opacity", 0.2 );
    shadowFuzzyness = conf.readEntry( "Fuzzyness", 10 );
    shadowSize = conf.readEntry( "Size", 4 );
    intensifyActiveShadow = conf.readEntry( "IntensifyActiveShadow", true );

    QString shadowtexture =  KGlobal::dirs()->findResource("data", "kwin/shadow-texture.png");
    mShadowTexture = new GLTexture(shadowtexture);
    }

QRect ShadowEffect::shadowRectangle(const QRect& windowRectangle) const
    {
    int shadowGrow = shadowFuzzyness + shadowSize + 20;
    return windowRectangle.adjusted( shadowXOffset - shadowGrow, shadowYOffset - shadowGrow,
            shadowXOffset + shadowGrow, shadowYOffset + shadowGrow);
    }

void ShadowEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( useShadow( w ))
        {
        data.mask |= PAINT_WINDOW_TRANSLUCENT;
        data.paint |= QRegion( shadowRectangle( ( QRegion( w->geometry()) & data.paint ).boundingRect() ));
        }
    effects->prePaintWindow( w, data, time );
    }

void ShadowEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( useShadow( w ))
        drawShadow( w, mask, region, data );
    effects->paintWindow( w, mask, region, data );
    }

void ShadowEffect::postPaintWindow( EffectWindow* w )
    {
    effects->postPaintWindow( w );
    }

QRect ShadowEffect::transformWindowDamage( EffectWindow* w, const QRect& r )
    {
    if( !useShadow( w ))
        return effects->transformWindowDamage( w, r );
    QRect r2 = r | shadowRectangle( r );
    return effects->transformWindowDamage( w, r2 );
    }

bool ShadowEffect::useShadow( EffectWindow* w ) const
    {
    return !w->isDeleted() && !w->isDesktop() && !w->isDock();
    }

void ShadowEffect::addQuadVertices(QVector<float>& verts, float x1, float y1, float x2, float y2) const
{
    verts << x1 << y1;
    verts << x1 << y2;
    verts << x2 << y2;
    verts << x2 << y1;
}

void ShadowEffect::drawShadow( EffectWindow* window, int mask, QRegion region, WindowPaintData& data )
    {
    if(( mask & PAINT_WINDOW_TRANSLUCENT ) == 0 )
        return;
    glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT );
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    int fuzzy = shadowFuzzyness;
    // Shadow's size must be a least 2*fuzzy in both directions (or the corners will be broken)
    int w = qMax(fuzzy*2, window->width() + 2*shadowSize);
    int h = qMax(fuzzy*2, window->height() + 2*shadowSize);

    glPushMatrix();
    if( mask & PAINT_WINDOW_TRANSFORMED )
        glTranslatef( data.xTranslate, data.yTranslate, 0 );
    glTranslatef( window->x() + shadowXOffset - qMax(0, w - window->width()) / 2.0,
                  window->y() + shadowYOffset - qMax(0, h - window->height()) / 2.0, 0 );
    if(( mask & PAINT_WINDOW_TRANSFORMED ) && ( data.xScale != 1 || data.yScale != 1 ))
        glScalef( data.xScale, data.yScale, 1 );

    QVector<float> verts, texcoords;
    // center
    addQuadVertices(verts, 0 + fuzzy, 0 + fuzzy, w - fuzzy, h - fuzzy);
    addQuadVertices(texcoords, 0.5, 0.5, 0.5, 0.5);
    // sides
    // left
    addQuadVertices(verts, 0 - fuzzy, 0 + fuzzy, 0 + fuzzy, h - fuzzy);
    addQuadVertices(texcoords, 0.0, 0.5, 0.5, 0.5);
    // top
    addQuadVertices(verts, 0 + fuzzy, 0 - fuzzy, w - fuzzy, 0 + fuzzy);
    addQuadVertices(texcoords, 0.5, 0.0, 0.5, 0.5);
    // right
    addQuadVertices(verts, w - fuzzy, 0 + fuzzy, w + fuzzy, h - fuzzy);
    addQuadVertices(texcoords, 0.5, 0.5, 1.0, 0.5);
    // bottom
    addQuadVertices(verts, 0 + fuzzy, h - fuzzy, w - fuzzy, h + fuzzy);
    addQuadVertices(texcoords, 0.5, 0.5, 0.5, 1.0);
    // corners
    // top-left
    addQuadVertices(verts, 0 - fuzzy, 0 - fuzzy, 0 + fuzzy, 0 + fuzzy);
    addQuadVertices(texcoords, 0.0, 0.0, 0.5, 0.5);
    // top-right
    addQuadVertices(verts, w - fuzzy, 0 - fuzzy, w + fuzzy, 0 + fuzzy);
    addQuadVertices(texcoords, 0.5, 0.0, 1.0, 0.5);
    // bottom-left
    addQuadVertices(verts, 0 - fuzzy, h - fuzzy, 0 + fuzzy, h + fuzzy);
    addQuadVertices(texcoords, 0.0, 0.5, 0.5, 1.0);
    // bottom-right
    addQuadVertices(verts, w - fuzzy, h - fuzzy, w + fuzzy, h + fuzzy);
    addQuadVertices(texcoords, 0.5, 0.5, 1.0, 1.0);

    mShadowTexture->bind();
    // Take the transparency settings and window's transparency into account.
    // Also make the shadow more transparent if we've made it bigger
    float opacity = shadowOpacity;
    if( intensifyActiveShadow && window == effects->activeWindow() )
    {
        opacity = 1 - (1 - shadowOpacity)*(1 - shadowOpacity);
    }
    glColor4f(0, 0, 0, opacity * data.opacity * (window->width() / (double)w) * (window->height() / (double)h));
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    // We have two elements per vertex in the verts array
    int verticesCount = verts.count() / 2;
    renderGLGeometry( mask, region, verticesCount, verts.data(), texcoords.data() );
    mShadowTexture->unbind();

    glPopMatrix();
    glPopAttrib();
    }

} // namespace
