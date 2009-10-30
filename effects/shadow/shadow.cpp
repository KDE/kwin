/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>
Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

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

#include "shadow.h"
#include "shadow_helper.h"

#include <kwinglutils.h>

#include <kconfiggroup.h>
#include <kdebug.h>
#include <KStandardDirs>
#include <kcolorscheme.h>
#include <KGlobalSettings>

#include <cmath>

namespace KWin
{

KWIN_EFFECT( shadow, ShadowEffect )

ShadowEffect::ShadowEffect()
    : shadowSize( 0 )
    {
    cachedBlendPicture = X::None;
    reconfigure( ReconfigureAll );
    connect(KGlobalSettings::self(), SIGNAL(kdisplayPaletteChanged()),
             this, SLOT(updateShadowColor()));
    }

ShadowEffect::~ShadowEffect()
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    for( int i = 0; i < mDefaultShadowTextures.size(); i++ )
        delete mDefaultShadowTextures.at( i );
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    for( int i = 0; i < mDefaultShadowPics.size(); i++ )
        delete mDefaultShadowPics.at( i );
#endif
    }

void ShadowEffect::reconfigure( ReconfigureFlags )
    {
    KConfigGroup conf = effects->effectConfig("Shadow");
    shadowXOffset = conf.readEntry( "XOffset", 0 );
    shadowYOffset = conf.readEntry( "YOffset", 3 );
    shadowOpacity = conf.readEntry( "Opacity", 0.25 );
    shadowFuzzyness = conf.readEntry( "Fuzzyness", 10 );
    shadowSize = conf.readEntry( "Size", 5 );
    intensifyActiveShadow = conf.readEntry( "IntensifyActiveShadow", true );
    updateShadowColor();

    // Load decoration shadow related things
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( effects->compositingType() == OpenGLCompositing )
        {
        // Delete any other textures in memory
        for( int i = 0; i < mDefaultShadowTextures.size(); i++ )
                delete mDefaultShadowTextures.at( i );
        mDefaultShadowTextures.clear();

        //-------------------------
        // Create default textures

        mDefaultShadowQuadType = effects->newWindowQuadType(); // TODO: Unregister?
        QImage shadowImageSrc( KGlobal::dirs()->findResource( "data", "kwin/shadow-texture.png" ));

        // Might as well process the shadow color here
        QImage shadowImage( shadowImageSrc.width(), shadowImageSrc.height(), QImage::Format_ARGB32 );
        QPainter painter( &shadowImage );
        //painter.setCompositionMode( QPainter::CompositionMode_Multiply );
        painter.setPen( Qt::NoPen );
        painter.setBrush( shadowColor );
        painter.drawRect( 0, 0, shadowImage.width(), shadowImage.height() );
        painter.end();
        shadowImage.setAlphaChannel( shadowImageSrc.alphaChannel() ); // Cheat, just use the alpha mask

        int hw = shadowImage.width() / 2;
        int hh = shadowImage.height() / 2;
        mDefaultShadowTextures.append( new GLTexture( shadowImage.copy( 0,  0,  hw, hh )));
        mDefaultShadowTextures.append( new GLTexture( shadowImage.copy( hw, 0,  1,  hh )));
        mDefaultShadowTextures.append( new GLTexture( shadowImage.copy( hw, 0,  hw, hh )));
        mDefaultShadowTextures.append( new GLTexture( shadowImage.copy( 0,  hh, hw, 1 )));
        mDefaultShadowTextures.append( new GLTexture( shadowImage.copy( hw, hh, 1,  1 )));
        mDefaultShadowTextures.append( new GLTexture( shadowImage.copy( hw, hh, hw, 1 )));
        mDefaultShadowTextures.append( new GLTexture( shadowImage.copy( 0,  hh, hw, hh )));
        mDefaultShadowTextures.append( new GLTexture( shadowImage.copy( hw, hh, 1,  hh )));
        mDefaultShadowTextures.append( new GLTexture( shadowImage.copy( hw, hh, hw, hh )));
        }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if( effects->compositingType() == XRenderCompositing )
        {
        // Delete any other pictures in memory
        for( int i = 0; i < mDefaultShadowPics.size(); i++ )
            delete mDefaultShadowPics.at( i );
        mDefaultShadowPics.clear();

        // Create default pictures
        mDefaultShadowQuadType = effects->newWindowQuadType(); // TODO: Unregister?
        QPixmap shadowPixmap( KGlobal::dirs()->findResource( "data", "kwin/shadow-texture.png" ));
        shadowPixmap = shadowPixmap.scaled( QSize( shadowFuzzyness * 4, shadowFuzzyness * 4 ),
            Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
        int hw = shadowPixmap.width() / 2;
        int hh = shadowPixmap.height() / 2;
        mDefaultShadowPics.append( new XRenderPicture( shadowPixmap.copy( 0,  0,  hw, hh )));
        mDefaultShadowPics.append( new XRenderPicture( shadowPixmap.copy( hw, 0,  1,  hh )));
        mDefaultShadowPics.append( new XRenderPicture( shadowPixmap.copy( hw, 0,  hw, hh )));
        mDefaultShadowPics.append( new XRenderPicture( shadowPixmap.copy( 0,  hh, hw, 1 )));
        mDefaultShadowPics.append( new XRenderPicture( shadowPixmap.copy( hw, hh, 1,  1 )));
        mDefaultShadowPics.append( new XRenderPicture( shadowPixmap.copy( hw, hh, hw, 1 )));
        mDefaultShadowPics.append( new XRenderPicture( shadowPixmap.copy( 0,  hh, hw, hh )));
        mDefaultShadowPics.append( new XRenderPicture( shadowPixmap.copy( hw, hh, 1,  hh )));
        mDefaultShadowPics.append( new XRenderPicture( shadowPixmap.copy( hw, hh, hw, hh )));

        // Apply repeat attribute to all pictures
        XRenderPictureAttributes pa;
        pa.repeat = true;
        for( int i = 0; i < mDefaultShadowPics.size(); i++ )
            XRenderChangePicture( display(), *mDefaultShadowPics.at( i ), CPRepeat, &pa );
        }
#endif

    // Force rebuild of all quads to clear their caches
    foreach( EffectWindow *w, effects->stackingOrder() )
        if( w ) // TODO/HACK: stackingOrder() should NOT return NULL
            w->buildQuads( true );
    }

void ShadowEffect::updateShadowColor()
    {
    KConfigGroup conf = effects->effectConfig("Shadow");
    shadowColor = conf.readEntry( "Color",  schemeShadowColor() );
    }

QRect ShadowEffect::shadowRectangle( EffectWindow* w, const QRect& windowRectangle ) const
    {
    Q_UNUSED( w );
    int shadowGrow = shadowFuzzyness + shadowSize;
    return windowRectangle.adjusted( shadowXOffset - shadowGrow, shadowYOffset - shadowGrow,
            shadowXOffset + shadowGrow, shadowYOffset + shadowGrow);
    }

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
static ScreenPaintData gScreenData;
#endif

void ShadowEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    shadowDatas.clear();
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if ((mask & PAINT_SCREEN_TRANSFORMED) &&
        (effects->compositingType() == XRenderCompositing)) // TODO: copy constructor?
        {
        gScreenData.xTranslate = data.xTranslate;
        gScreenData.yTranslate = data.yTranslate;
        gScreenData.xScale = data.xScale;
        gScreenData.yScale = data.yScale;
        }
#endif
    // Draw windows
    effects->paintScreen( mask, region, data );

    // Draw shadows
    drawQueuedShadows( 0 );
    }

void ShadowEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( useShadow( w ))
        data.paint |= shadowRectangle( w, data.paint.boundingRect() );
    effects->prePaintWindow( w, data, time );
    }

void ShadowEffect::drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    // Whether the shadow drawing can be delayed or not.
    bool optimize = !(mask & ( PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_TRANSFORMED |
            PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS | PAINT_WINDOW_TRANSLUCENT | PAINT_DECORATION_ONLY ));

    QRegion rgn = region;
    if ( w->keepAbove() )
        {
            QRegion shape = w->shape().translated( w->x(), w->y());
            if (optimize)
                {
                    rgn -= shape;
                    QList<ShadowData>::iterator i;
                    for( i = shadowDatas.begin(); i != shadowDatas.end(); ++i )
                        (*i).clip |= shape;
                }
            optimize = false;
        }
    if( !optimize )
        {
        // Transformed or translucent windows are drawn bottom-to-top, so
        //  first we need to draw all queued shadows.
        drawQueuedShadows( w );
        }
    if( useShadow( w ) && (!(mask & PAINT_DECORATION_ONLY) || w->keepAbove()) )
        {
        if( !optimize )
            {
            // For translucent windows, shadow needs to be drawn before the
            //  window itself.
            drawShadow( w, mask, rgn, data );
            }
        else
            {
            // For opaque windows, just schedule the shadow to be drawn later
            ShadowData d(w, data);
            d.clip = w->shape().translated( w->x(), w->y());
            if( !shadowDatas.isEmpty())
                d.clip |= shadowDatas.last().clip;
            d.mask = mask;
            foreach( const QRect &r, region.rects() )
                d.region |= shadowRectangle( w, r );
            d.region &= region;
            shadowDatas.append(d);
            }
        }

    effects->drawWindow( w, mask, region, data );
    }

void ShadowEffect::buildQuads( EffectWindow* w, WindowQuadList& quadList )
    {
    // TODO: add config option to not have shadows for menus, etc.
    // Make our own shadow as the decoration doesn't support it
    int fuzzy = shadowFuzzyness;
    // Shadow's size must be a least 2*fuzzy in both directions (or the corners will be broken)
    int width = qMax( fuzzy * 2, w->width() + 2 * shadowSize );
    int height = qMax( fuzzy * 2, w->height() + 2 * shadowSize );
    double x1, y1, x2, y2;
    int id = 0;
    // top-left
    x1 = shadowXOffset - shadowSize + 0 - fuzzy;
    y1 = shadowYOffset - shadowSize + 0 - fuzzy;
    x2 = shadowXOffset - shadowSize + 0 + fuzzy;
    y2 = shadowYOffset - shadowSize + 0 + fuzzy;
    WindowQuad topLeftQuad( mDefaultShadowQuadType, id++ );
    topLeftQuad[ 0 ] = WindowVertex( x1, y1, 0, 1 );
    topLeftQuad[ 1 ] = WindowVertex( x2, y1, 1, 1 );
    topLeftQuad[ 2 ] = WindowVertex( x2, y2, 1, 0 );
    topLeftQuad[ 3 ] = WindowVertex( x1, y2, 0, 0 );
    quadList.append( topLeftQuad );
    // top
    x1 = shadowXOffset - shadowSize + 0 + fuzzy;
    y1 = shadowYOffset - shadowSize + 0 - fuzzy;
    x2 = shadowXOffset - shadowSize + width - fuzzy;
    y2 = shadowYOffset - shadowSize + 0 + fuzzy;
    WindowQuad topQuad( mDefaultShadowQuadType, id++ );
    topQuad[ 0 ] = WindowVertex( x1, y1, 0, 1 );
    topQuad[ 1 ] = WindowVertex( x2, y1, 1, 1 );
    topQuad[ 2 ] = WindowVertex( x2, y2, 1, 0 );
    topQuad[ 3 ] = WindowVertex( x1, y2, 0, 0 );
    quadList.append( topQuad );
    // top-right
    x1 = shadowXOffset - shadowSize + width - fuzzy;
    y1 = shadowYOffset - shadowSize + 0 - fuzzy;
    x2 = shadowXOffset - shadowSize + width + fuzzy;
    y2 = shadowYOffset - shadowSize + 0 + fuzzy;
    WindowQuad topRightQuad( mDefaultShadowQuadType, id++ );
    topRightQuad[ 0 ] = WindowVertex( x1, y1, 0, 1 );
    topRightQuad[ 1 ] = WindowVertex( x2, y1, 1, 1 );
    topRightQuad[ 2 ] = WindowVertex( x2, y2, 1, 0 );
    topRightQuad[ 3 ] = WindowVertex( x1, y2, 0, 0 );
    quadList.append( topRightQuad );
    // left
    x1 = shadowXOffset - shadowSize + 0 - fuzzy;
    y1 = shadowYOffset - shadowSize + 0 + fuzzy;
    x2 = shadowXOffset - shadowSize + 0 + fuzzy;
    y2 = shadowYOffset - shadowSize + height - fuzzy;
    WindowQuad leftQuad( mDefaultShadowQuadType, id++ );
    leftQuad[ 0 ] = WindowVertex( x1, y1, 0, 1 );
    leftQuad[ 1 ] = WindowVertex( x2, y1, 1, 1 );
    leftQuad[ 2 ] = WindowVertex( x2, y2, 1, 0 );
    leftQuad[ 3 ] = WindowVertex( x1, y2, 0, 0 );
    quadList.append( leftQuad );
    // center
    x1 = shadowXOffset - shadowSize + 0 + fuzzy;
    y1 = shadowYOffset - shadowSize + 0 + fuzzy;
    x2 = shadowXOffset - shadowSize + width - fuzzy;
    y2 = shadowYOffset - shadowSize + height - fuzzy;
    WindowQuad contentsQuad( mDefaultShadowQuadType, id++ );
    contentsQuad[ 0 ] = WindowVertex( x1, y1, 0, 1 );
    contentsQuad[ 1 ] = WindowVertex( x2, y1, 1, 1 );
    contentsQuad[ 2 ] = WindowVertex( x2, y2, 1, 0 );
    contentsQuad[ 3 ] = WindowVertex( x1, y2, 0, 0 );
    quadList.append( contentsQuad );
    // right
    x1 = shadowXOffset - shadowSize + width - fuzzy;
    y1 = shadowYOffset - shadowSize + 0 + fuzzy;
    x2 = shadowXOffset - shadowSize + width + fuzzy;
    y2 = shadowYOffset - shadowSize + height - fuzzy;
    WindowQuad rightQuad( mDefaultShadowQuadType, id++ );
    rightQuad[ 0 ] = WindowVertex( x1, y1, 0, 1 );
    rightQuad[ 1 ] = WindowVertex( x2, y1, 1, 1 );
    rightQuad[ 2 ] = WindowVertex( x2, y2, 1, 0 );
    rightQuad[ 3 ] = WindowVertex( x1, y2, 0, 0 );
    quadList.append( rightQuad );
    // bottom-left
    x1 = shadowXOffset - shadowSize + 0 - fuzzy;
    y1 = shadowYOffset - shadowSize + height - fuzzy;
    x2 = shadowXOffset - shadowSize + 0 + fuzzy;
    y2 = shadowYOffset - shadowSize + height + fuzzy;
    WindowQuad bottomLeftQuad( mDefaultShadowQuadType, id++ );
    bottomLeftQuad[ 0 ] = WindowVertex( x1, y1, 0, 1 );
    bottomLeftQuad[ 1 ] = WindowVertex( x2, y1, 1, 1 );
    bottomLeftQuad[ 2 ] = WindowVertex( x2, y2, 1, 0 );
    bottomLeftQuad[ 3 ] = WindowVertex( x1, y2, 0, 0 );
    quadList.append( bottomLeftQuad );
    // bottom
    x1 = shadowXOffset - shadowSize + 0 + fuzzy;
    y1 = shadowYOffset - shadowSize + height - fuzzy;
    x2 = shadowXOffset - shadowSize + width - fuzzy;
    y2 = shadowYOffset - shadowSize + height + fuzzy;
    WindowQuad bottomQuad( mDefaultShadowQuadType, id++ );
    bottomQuad[ 0 ] = WindowVertex( x1, y1, 0, 1 );
    bottomQuad[ 1 ] = WindowVertex( x2, y1, 1, 1 );
    bottomQuad[ 2 ] = WindowVertex( x2, y2, 1, 0 );
    bottomQuad[ 3 ] = WindowVertex( x1, y2, 0, 0 );
    quadList.append( bottomQuad );
    // bottom-right
    x1 = shadowXOffset - shadowSize + width - fuzzy;
    y1 = shadowYOffset - shadowSize + height - fuzzy;
    x2 = shadowXOffset - shadowSize + width + fuzzy;
    y2 = shadowYOffset - shadowSize + height + fuzzy;
    WindowQuad bottomRightQuad( mDefaultShadowQuadType, id++ );
    bottomRightQuad[ 0 ] = WindowVertex( x1, y1, 0, 1 );
    bottomRightQuad[ 1 ] = WindowVertex( x2, y1, 1, 1 );
    bottomRightQuad[ 2 ] = WindowVertex( x2, y2, 1, 0 );
    bottomRightQuad[ 3 ] = WindowVertex( x1, y2, 0, 0 );
    quadList.append( bottomRightQuad );

    effects->buildQuads( w, quadList );
    }

QRect ShadowEffect::transformWindowDamage( EffectWindow* w, const QRect& r )
    {
    if( !useShadow( w ))
        return effects->transformWindowDamage( w, r );
    QRect r2 = r | shadowRectangle( w, r );
    return effects->transformWindowDamage( w, r2 );
    }

void ShadowEffect::windowClosed( EffectWindow* c )
    {
    effects->addRepaint( shadowRectangle( c, c->geometry() ));
    }

bool ShadowEffect::useShadow( EffectWindow* w ) const
    {
    return !w->isDeleted() && !w->isDesktop() && !w->isDock()
        // popups may have shadow even if shaped, their shape is almost rectangular
        && ( !w->hasOwnShape() || w->isDropdownMenu() || w->isPopupMenu() || w->isComboBox())
        // If decoration has it's own shadow leave it alone
        && !( w->hasDecoration() && effects->hasDecorationShadows() );
    }

void ShadowEffect::addQuadVertices(QVector<float>& verts, float x1, float y1, float x2, float y2) const
    {
    verts << x1 << y1;
    verts << x1 << y2;
    verts << x2 << y2;
    verts << x2 << y1;
    }

void ShadowEffect::drawQueuedShadows( EffectWindow* behindWindow )
    {
    QList<ShadowData> newShadowDatas;
    QList<ShadowData> thisTime;
    EffectWindowList stack = effects->stackingOrder();
    foreach( const ShadowData &d, shadowDatas )
        {
        // If behindWindow is given then only render shadows of the windows
        //  that are behind that window.
        if( !behindWindow || stack.indexOf(d.w) < stack.indexOf(behindWindow))
            thisTime.append(d);
        else
            newShadowDatas.append(d);
        }
    if( thisTime.count() )
        { // Render them in stacking order
        foreach( EffectWindow *w, stack )
            for( int i = 0; i < thisTime.size(); i++ )
                { // Cannot use foreach() due to thisTime.removeOne()
                const ShadowData d = thisTime.at(i);
                if( d.w == w )
                    {
                    drawShadow( d.w, d.mask,
                        d.region.subtracted( d.clip ), d.data );
                    thisTime.removeAt( i );
                    break;
                    }
                }
        }
    // Render the rest on the top (For menus, etc.)
    foreach( const ShadowData &d, thisTime )
        drawShadow( d.w, d.mask, d.region.subtracted( d.clip ), d.data );
    shadowDatas = newShadowDatas;
    }

// Modified version of SceneOpenGL::Window::prepareRenderStates() from scene_opengl.cpp
void ShadowEffect::prepareRenderStates( GLTexture *texture, double opacity, double brightness, double saturation )
    {
    Q_UNUSED( texture );
    Q_UNUSED( saturation );
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    // setup blending of transparent windows
    glPushAttrib( GL_ENABLE_BIT );
    /*if( saturation != 1.0 && texture->saturationSupported() )
        {
        // First we need to get the color from [0; 1] range to [0.5; 1] range
        glActiveTexture( GL_TEXTURE0 );
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE );
        glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA );
        const float scale_constant[] = { 1.0, 1.0, 1.0, 0.5 };
        glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, scale_constant );
        texture->bind();

        // Then we take dot product of the result of previous pass and
        //  saturation_constant. This gives us completely unsaturated
        //  (greyscale) image
        // Note that both operands have to be in range [0.5; 1] since opengl
        //  automatically substracts 0.5 from them
        glActiveTexture( GL_TEXTURE1 );
        float saturation_constant[] = { 0.5 + 0.5*0.30, 0.5 + 0.5*0.59, 0.5 + 0.5*0.11, saturation };
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE );
        glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_DOT3_RGB );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR );
        glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, saturation_constant );
        texture->bind();

        // Finally we need to interpolate between the original image and the
        //  greyscale image to get wanted level of saturation
        glActiveTexture( GL_TEXTURE2 );
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE );
        glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0 );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA );
        glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, saturation_constant );
        // Also replace alpha by primary color's alpha here
        glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA );
        // And make primary color contain the wanted opacity
        glColor4f( opacity, opacity, opacity, opacity );
        texture->bind();

        if( brightness != 1.0 )
            {
            glActiveTexture( GL_TEXTURE3 );
            glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE );
            glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE );
            glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS );
            glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR );
            glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR );
            glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR );
            // The color has to be multiplied by both opacity and brightness
            float opacityByBrightness = opacity * brightness;
            glColor4f( opacityByBrightness, opacityByBrightness, opacityByBrightness, opacity );
            glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE );
            glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS );
            glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA );
            texture->bind();
            }

        glActiveTexture(GL_TEXTURE0 );
        }
    else*/ if( opacity != 1.0 || brightness != 1.0 )
        { // The window is additionally configured to have its opacity adjusted
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
        glColor4f( brightness, brightness, brightness, opacity );
        }
#endif
    }

// Modified version of SceneOpenGL::Window::restoreRenderStates() from scene_opengl.cpp
void ShadowEffect::restoreRenderStates( GLTexture *texture, double opacity, double brightness, double saturation )
    {
    Q_UNUSED( texture );
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( opacity != 1.0 || saturation != 1.0 || brightness != 1.0 )
        {
        /*if( saturation != 1.0 && texture->saturationSupported())
            {
            glActiveTexture(GL_TEXTURE3);
            glDisable( texture->target() );
            glActiveTexture(GL_TEXTURE2);
            glDisable( texture->target() );
            glActiveTexture(GL_TEXTURE1);
            glDisable( texture->target() );
            glActiveTexture(GL_TEXTURE0);
            }*/
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
        glColor4f( 0.0, 0.0, 0.0, 0.0 );
        }

    glPopAttrib();  // ENABLE_BIT
#endif
    }

void ShadowEffect::drawShadowQuadOpenGL( GLTexture *texture, QVector<float> verts, QVector<float> texCoords,
    QRegion region, float opacity, float brightness, float saturation,  GLShader* shader )
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    glColor4f( 1.0, 1.0, 1.0, 1.0 );
    glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
    prepareRenderStates( texture, opacity, brightness, saturation ); // Usually overrides the above
    if( shader )
        {
        shader->setUniform("opacity", opacity);
        shader->setUniform("saturation", saturation);
        shader->setUniform("brightness", brightness);
        shader->setUniform("textureWidth", 1.0f);
        shader->setUniform("textureHeight", 1.0f);
        }
    texture->bind();
    texture->enableNormalizedTexCoords();
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    renderGLGeometry( region, 4, verts.data(), texCoords.data() );
    texture->disableNormalizedTexCoords();
    texture->unbind();
    restoreRenderStates( texture, opacity, brightness, saturation );
#endif
    }

void ShadowEffect::drawShadowQuadXRender( XRenderPicture *picture, QRect rect, float xScale, float yScale,
    QColor color, float opacity, float brightness, float saturation )
    {
    Q_UNUSED( saturation );
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    XRenderPicture fill;
    if( color.isValid() )
        {
        if ( color == cachedColor && cachedBlendPicture != X::None )
            fill = cachedBlendPicture;
        else
            {
            XRenderColor xc = preMultiply( color, opacity );
            cachedBlendPicture = fill = xRenderFill( &xc );
            cachedColor = color;
            }
        }
    else
        fill = xRenderBlendPicture(opacity);

    // Scale if required
    if( xScale != 1.0 || yScale != 1.0 )
        {
        XTransform xform = {{
            { XDoubleToFixed( 1.0 / xScale ), XDoubleToFixed( 0.0 ), XDoubleToFixed( 0.0 ) },
            { XDoubleToFixed( 0.0 ), XDoubleToFixed( 1.0 / yScale ), XDoubleToFixed( 0.0 ) },
            { XDoubleToFixed( 0.0 ), XDoubleToFixed( 0.0 ), XDoubleToFixed( 1.0 ) }
        }};
        XRenderSetPictureTransform( display(), *picture, &xform );
        }

    // Render it
    // TODO: This always uses the fast filter, detect when to use smooth instead
    if( color.isValid() )
        XRenderComposite( display(), PictOpOver, fill, *picture, effects->xrenderBufferPicture(), 0, 0, 0, 0,
            rect.x(), rect.y(), rect.width(), rect.height() );
    else
        XRenderComposite( display(), PictOpOver, *picture, fill, effects->xrenderBufferPicture(), 0, 0, 0, 0,
            rect.x(), rect.y(), rect.width(), rect.height() );

    // Fake brightness by overlaying black
    // Cannot use XRenderFillRectangle() due to ARGB
    fill = xRenderBlendPicture(( 1 - brightness ) * opacity);
    XRenderComposite( display(), PictOpOver, fill, *picture, effects->xrenderBufferPicture(), 0, 0, 0, 0,
        rect.x(), rect.y(), rect.width(), rect.height() );

    // Return to scale to 1.0
    if( xScale != 1.0 || yScale != 1.0 )
        {
        XTransform xform = {{
            { XDoubleToFixed( 1.0 ), XDoubleToFixed( 0.0 ), XDoubleToFixed( 0.0 ) },
            { XDoubleToFixed( 0.0 ), XDoubleToFixed( 1.0 ), XDoubleToFixed( 0.0 ) },
            { XDoubleToFixed( 0.0 ), XDoubleToFixed( 0.0 ), XDoubleToFixed( 1.0 ) }
        }};
        XRenderSetPictureTransform( display(), *picture, &xform );
        }
#endif
    }

void ShadowEffect::drawShadow( EffectWindow* window, int mask, QRegion region, const WindowPaintData& data )
    {
    // Don't allow windows to cast shadows on other displays
    QRegion clipperGeom;
    for( int screen = 0; screen < effects->numScreens(); screen++ )
        {
        QRect screenGeom = effects->clientArea( ScreenArea, screen, 0 );
        if( !( window->geometry() & screenGeom ).isNull() )
            clipperGeom |= screenGeom;
        }
    PaintClipper pc( clipperGeom );

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( effects->compositingType() == OpenGLCompositing )
        {
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

        foreach( const WindowQuad &quad, data.quads )
            {
            if( quad.type() != mDefaultShadowQuadType )
                continue; // Not a shadow quad

            glPushMatrix();

            // Use the window's top-left as the origin
            glTranslatef( window->x(), window->y(), 0 );
            if( mask & PAINT_WINDOW_TRANSFORMED )
                glTranslatef( data.xTranslate, data.yTranslate, data.zTranslate );
            if(( mask & PAINT_WINDOW_TRANSFORMED ) && ( data.xScale != 1 || data.yScale != 1 ))
                glScalef( data.xScale, data.yScale, data.zScale );
            if(( mask & PAINT_WINDOW_TRANSFORMED ) && data.rotation )
                {
                glTranslatef( data.rotation->xRotationPoint,
                    data.rotation->yRotationPoint,
                    data.rotation->zRotationPoint );
                float xAxis = 0.0;
                float yAxis = 0.0;
                float zAxis = 0.0;
                switch( data.rotation->axis )
                    {
                    case RotationData::XAxis:
                        xAxis = 1.0;
                        break;
                    case RotationData::YAxis:
                        yAxis = 1.0;
                        break;
                    case RotationData::ZAxis:
                        zAxis = 1.0;
                        break;
                    }
                glRotatef( data.rotation->angle, xAxis, yAxis, zAxis );
                glTranslatef( -data.rotation->xRotationPoint,
                    -data.rotation->yRotationPoint,
                    -data.rotation->zRotationPoint );
                }

            // Create our polygon
            QVector<float> verts, texcoords;
            verts.reserve(8);
            texcoords.reserve(8);
            verts << quad[0].x() << quad[0].y();
            verts << quad[1].x() << quad[1].y();
            verts << quad[2].x() << quad[2].y();
            verts << quad[3].x() << quad[3].y();
            texcoords << quad[0].textureX() << quad[0].textureY();
            texcoords << quad[1].textureX() << quad[1].textureY();
            texcoords << quad[2].textureX() << quad[2].textureY();
            texcoords << quad[3].textureX() << quad[3].textureY();

            // Work out which texture to use
            float opacity = shadowOpacity;
            if( intensifyActiveShadow && window == effects->activeWindow() )
                opacity = 1 - ( 1 - shadowOpacity ) * ( 1 - shadowOpacity );

            drawShadowQuadOpenGL( mDefaultShadowTextures.at( quad.id() ),
                verts, texcoords, region,
                data.opacity * opacity,
                data.brightness,
                data.saturation,
                data.shader);

            glPopMatrix();
            }

        glPopAttrib();
        }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if( effects->compositingType() == XRenderCompositing )
        {
        XRenderSetPictureClipRegion( display(), effects->xrenderBufferPicture(), region.handle() );

        foreach( const WindowQuad &quad, data.quads )
            {
            if( quad.type() != mDefaultShadowQuadType )
                continue; // Not a shadow quad

            // Determine transformed quad position
            QRect windowRect = window->geometry();
            float xScale = 1.0;
            float yScale = 1.0;
            float xTranslate = 0.0;
            float yTranslate = 0.0;
            if( mask & PAINT_SCREEN_TRANSFORMED)
                {
                xScale = gScreenData.xScale;
                yScale = gScreenData.yScale;
                xTranslate += ( xScale - 1.0 ) * windowRect.x() + gScreenData.xTranslate;
                yTranslate += ( yScale - 1.0 ) * windowRect.y() + gScreenData.yTranslate;
                }
            if( mask & PAINT_WINDOW_TRANSFORMED)
                {
                xTranslate += xScale * data.xTranslate;
                yTranslate += yScale * data.yTranslate;
                xScale *= data.xScale;
                yScale *= data.yScale;
                }
            QRect quadRect(
                window->x() + quad[0].x() * xScale + xTranslate,
                window->y() + quad[0].y() * yScale + yTranslate,
                ( quad[2].x() - quad[0].x() ) * xScale,
                ( quad[2].y() - quad[0].y() ) * yScale );

            // Work out which texture to use
            float opacity = shadowOpacity;
            if( intensifyActiveShadow && window == effects->activeWindow() )
                opacity = 1 - ( 1 - shadowOpacity ) * ( 1 - shadowOpacity );

            drawShadowQuadXRender( mDefaultShadowPics.at( quad.id() ), quadRect, xScale, yScale,
                shadowColor, opacity * data.opacity, data.brightness, data.saturation );
            }
        }
#endif
    }

} // namespace

