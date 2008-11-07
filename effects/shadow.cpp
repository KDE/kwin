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
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <QRadialGradient>
#endif

#include <cmath>

namespace KWin
{

KWIN_EFFECT( shadow, ShadowEffect )

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
ShadowTiles::ShadowTiles(const QPixmap& shadow)
    {
    int w = shadow.width() / 2, h = shadow.height() / 2;
    cornerSize = QSize(w, h);
#define DUMP_CNR(_TILE_, _W_, _H_, _XOFF_, _YOFF_)\
    dump = QPixmap(_W_, _H_);\
    dump.fill(Qt::transparent);\
    p.begin(&dump);\
    p.drawPixmap( 0, 0, shadow, _XOFF_, _YOFF_, _W_, _H_ );\
    p.end();\
    _TILE_ = dump

    QPixmap dump; QPainter p;
    DUMP_CNR(topLeft, w, h, 0, 0);
    DUMP_CNR(topRight, w, h, w, 0);
    DUMP_CNR(btmLeft, w, h, 0, h);
    DUMP_CNR(btmRight, w, h, w, h);

    XRenderPictureAttributes pa; pa.repeat = True;
#define DUMP_TILE(_TILE_, _W_, _H_, _XOFF_, _YOFF_)\
    DUMP_CNR(_TILE_, _W_, _H_, _XOFF_, _YOFF_);\
    XRenderChangePicture (display(), _TILE_, CPRepeat, &pa)

    DUMP_TILE(top, 1, h, w, 0);
    DUMP_TILE(btm, 1, h, w, h);
    DUMP_TILE(left, w, 1, 0, h);
    DUMP_TILE(right, w, 1, w, h);

    DUMP_TILE(center, 1, 1, w, h);
    }

#undef DUMP_CNR
#undef DUMP_TILE

#endif

ShadowEffect::ShadowEffect()
    : shadowSize( 0 )
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    , mShadowPics( NULL )
#endif
    {
    reconfigure( ReconfigureAll );
    connect(KGlobalSettings::self(), SIGNAL(kdisplayPaletteChanged()),
             this, SLOT(updateShadowColor()));
    }

ShadowEffect::~ShadowEffect()
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    for( int i = 0; i < mShadowTextures.size(); i++ )
        for( int j = 0; j < mShadowTextures.at( i ).size(); j++ )
            delete mShadowTextures.at( i ).at( j );
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    delete mShadowPics;
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
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    delete mShadowPics;
    mShadowPics = NULL;
    if ( effects->compositingType() == XRenderCompositing)
        {
        qreal size = 2*(shadowFuzzyness+shadowSize)+1;
        QPixmap *shadow = new QPixmap(size, size); shadow->fill(Qt::transparent);
        size /= 2.0;
        QRadialGradient rg(size, size, size);
        QColor c(0,0,0,255);
        rg.setColorAt(0, c);
        c.setAlpha(0.3*c.alpha());
        if (shadowSize > 0)
            rg.setColorAt(((float)shadowSize)/(shadowFuzzyness+shadowSize), c);
        c.setAlpha(0); rg.setColorAt(0.8, c);
        QPainter p(shadow);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen); p.setBrush(rg);
        p.drawRect(shadow->rect());
        p.end();

        mShadowPics = new ShadowTiles(*shadow);
        delete shadow;
        }
#endif
    updateShadowColor();

    // Load decoration shadow related things
    bool reconfiguring = false;
    if( mShadowQuadTypes.count() )
        reconfiguring = true;
    mShadowQuadTypes.clear(); // Changed decoration? TODO: Unregister?
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( effects->compositingType() == OpenGLCompositing )
        {
        for( int i = 0; i < mShadowTextures.size(); i++ )
            for( int j = 0; j < mShadowTextures.at( i ).size(); j++ )
                delete mShadowTextures.at( i ).at( j );
        mShadowTextures.clear();
        if( effects->hasDecorationShadows() )
            {
            QList< QList<QImage> > shadowTextures = effects->shadowTextures();
            for( int i = 0; i < shadowTextures.size(); i++ )
                {
                mShadowQuadTypes.append( effects->newWindowQuadType() );
                QList<GLTexture*> textures;
                for( int j = 0; j < shadowTextures.at( i ).size(); j++ )
                    textures.append( new GLTexture( shadowTextures.at( i ).at( j )));
                mShadowTextures.append( textures );
                }
            }

        mDefaultShadowQuadType = effects->newWindowQuadType(); // TODO: Unregister?
        QImage shadowTexture( KGlobal::dirs()->findResource( "data", "kwin/shadow-texture.png" ));
        int hw = shadowTexture.width() / 2;
        int hh = shadowTexture.height() / 2;
        mDefaultShadowTextures.clear();
        mDefaultShadowTextures.append( new GLTexture( shadowTexture.copy( 0,  0,  hw, hh )));
        mDefaultShadowTextures.append( new GLTexture( shadowTexture.copy( hw, 0,  1,  hh )));
        mDefaultShadowTextures.append( new GLTexture( shadowTexture.copy( hw, 0,  hw, hh )));
        mDefaultShadowTextures.append( new GLTexture( shadowTexture.copy( 0,  hh, hw, 1 )));
        mDefaultShadowTextures.append( new GLTexture( shadowTexture.copy( hw, hh, 1,  1 )));
        mDefaultShadowTextures.append( new GLTexture( shadowTexture.copy( hw, hh, hw, 1 )));
        mDefaultShadowTextures.append( new GLTexture( shadowTexture.copy( 0,  hh, hw, hh )));
        mDefaultShadowTextures.append( new GLTexture( shadowTexture.copy( hw, hh, 1,  hh )));
        mDefaultShadowTextures.append( new GLTexture( shadowTexture.copy( hw, hh, hw, hh )));
        }
#endif

    if( reconfiguring )
        { // Force rebuild of all quads to clear their caches
        foreach( EffectWindow *w, effects->stackingOrder() )
            w->buildQuads( true );
        }
    }

void ShadowEffect::updateShadowColor()
    {
    KConfigGroup conf = effects->effectConfig("Shadow");
    shadowColor = conf.readEntry( "Color",  schemeShadowColor() );
    }

QRect ShadowEffect::shadowRectangle(const QRect& windowRectangle) const
    {
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
        {
        data.paint |= shadowRectangle( data.paint.boundingRect() );
        }
    effects->prePaintWindow( w, data, time );
    }

void ShadowEffect::drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    // Whether the shadow drawing can be delayed or not.
    bool optimize = !( mask & ( PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_TRANSFORMED |
            PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS | PAINT_WINDOW_TRANSLUCENT ));
    if( !optimize )
        {
        // Transformed or translucent windows are drawn bottom-to-top, so
        //  first we need to draw all queued shadows.
        drawQueuedShadows( w );
        }
    if( useShadow( w ))
        {
        if( !optimize )
            {
            // For translucent windows, shadow needs to be drawn before the
            //  window itself.
            drawShadow( w, mask, region, data );
            }
        else
            {
            // For opaque windows, just schedule the shadow to be drawn later
            ShadowData d(w, data);
            d.clip = w->shape().translated( w->x(), w->y());
            if( !shadowDatas.isEmpty())
                d.clip |= shadowDatas.last().clip;
            d.mask = mask;
            foreach(const QRect &r, region.rects())
                d.region |= shadowRectangle(r);
            d.region &= region;
            shadowDatas.append(d);
            }
        }

    effects->drawWindow( w, mask, region, data );
    }

void ShadowEffect::buildQuads( EffectWindow* w, WindowQuadList& quadList )
    {
    if( effects->compositingType() == XRenderCompositing )
        return; // TODO: Disable quad-based shadows in XRender mode for the moment
    bool shadowDefined = false;
    if( effects->hasDecorationShadows() )
        {
        // TODO: shadowQuads() is allowed to return different quads for
        //       active and inactive shadows. Is implementing it worth
        //       the performance drop?
        int id = 0;
        if( w->hasDecoration() )
            { // Decorated windows must be normal windows
            foreach( const QRect &r, w->shadowQuads( ShadowBorderedActive ))
                {
                shadowDefined = true;
                WindowQuad quad( mShadowQuadTypes.at( effects->shadowTextureList( ShadowBorderedActive )), id++ );
                quad[ 0 ] = WindowVertex( r.x(), r.y(), 0, 0 );
                quad[ 1 ] = WindowVertex( r.x() + r.width(), r.y(), 1, 0 );
                quad[ 2 ] = WindowVertex( r.x() + r.width(), r.y() + r.height(), 1, 1 );
                quad[ 3 ] = WindowVertex( r.x(), r.y() + r.height(), 0, 1 );
                quadList.append( quad );
                }
            }
        else if( w->isNormalWindow() )
            { // No decoration on a normal window
            foreach( const QRect &r, w->shadowQuads( ShadowBorderlessActive ))
                {
                shadowDefined = true;
                WindowQuad quad( mShadowQuadTypes.at( effects->shadowTextureList( ShadowBorderlessActive )), id++ );
                quad[ 0 ] = WindowVertex( r.x(), r.y(), 0, 0 );
                quad[ 1 ] = WindowVertex( r.x() + r.width(), r.y(), 1, 0 );
                quad[ 2 ] = WindowVertex( r.x() + r.width(), r.y() + r.height(), 1, 1 );
                quad[ 3 ] = WindowVertex( r.x(), r.y() + r.height(), 0, 1 );
                quadList.append( quad );
                }
            }
        else
            { // All other undecorated windows
            foreach( const QRect &r, w->shadowQuads( ShadowOther ))
                {
                shadowDefined = true;
                WindowQuad quad( mShadowQuadTypes.at( effects->shadowTextureList( ShadowOther )), id++ );
                quad[ 0 ] = WindowVertex( r.x(), r.y(), 0, 0 );
                quad[ 1 ] = WindowVertex( r.x() + r.width(), r.y(), 1, 0 );
                quad[ 2 ] = WindowVertex( r.x() + r.width(), r.y() + r.height(), 1, 1 );
                quad[ 3 ] = WindowVertex( r.x(), r.y() + r.height(), 0, 1 );
                quadList.append( quad );
                }
            }
        }
    if( !shadowDefined )
        {
        //TODO: add config option to not have shadows for menus, etc.
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
        topLeftQuad[ 0 ] = WindowVertex( x1, y1, 0, 0 );
        topLeftQuad[ 1 ] = WindowVertex( x2, y1, 1, 0 );
        topLeftQuad[ 2 ] = WindowVertex( x2, y2, 1, 1 );
        topLeftQuad[ 3 ] = WindowVertex( x1, y2, 0, 1 );
        quadList.append( topLeftQuad );
        // top
        x1 = shadowXOffset - shadowSize + 0 + fuzzy;
        y1 = shadowYOffset - shadowSize + 0 - fuzzy;
        x2 = shadowXOffset - shadowSize + width - fuzzy;
        y2 = shadowYOffset - shadowSize + 0 + fuzzy;
        WindowQuad topQuad( mDefaultShadowQuadType, id++ );
        topQuad[ 0 ] = WindowVertex( x1, y1, 0, 0 );
        topQuad[ 1 ] = WindowVertex( x2, y1, 1, 0 );
        topQuad[ 2 ] = WindowVertex( x2, y2, 1, 1 );
        topQuad[ 3 ] = WindowVertex( x1, y2, 0, 1 );
        quadList.append( topQuad );
        // top-right
        x1 = shadowXOffset - shadowSize + width - fuzzy;
        y1 = shadowYOffset - shadowSize + 0 - fuzzy;
        x2 = shadowXOffset - shadowSize + width + fuzzy;
        y2 = shadowYOffset - shadowSize + 0 + fuzzy;
        WindowQuad topRightQuad( mDefaultShadowQuadType, id++ );
        topRightQuad[ 0 ] = WindowVertex( x1, y1, 0, 0 );
        topRightQuad[ 1 ] = WindowVertex( x2, y1, 1, 0 );
        topRightQuad[ 2 ] = WindowVertex( x2, y2, 1, 1 );
        topRightQuad[ 3 ] = WindowVertex( x1, y2, 0, 1 );
        quadList.append( topRightQuad );
        // left
        x1 = shadowXOffset - shadowSize + 0 - fuzzy;
        y1 = shadowYOffset - shadowSize + 0 + fuzzy;
        x2 = shadowXOffset - shadowSize + 0 + fuzzy;
        y2 = shadowYOffset - shadowSize + height - fuzzy;
        WindowQuad leftQuad( mDefaultShadowQuadType, id++ );
        leftQuad[ 0 ] = WindowVertex( x1, y1, 0, 0 );
        leftQuad[ 1 ] = WindowVertex( x2, y1, 1, 0 );
        leftQuad[ 2 ] = WindowVertex( x2, y2, 1, 1 );
        leftQuad[ 3 ] = WindowVertex( x1, y2, 0, 1 );
        quadList.append( leftQuad );
        // center
        x1 = shadowXOffset - shadowSize + 0 + fuzzy;
        y1 = shadowYOffset - shadowSize + 0 + fuzzy;
        x2 = shadowXOffset - shadowSize + width - fuzzy;
        y2 = shadowYOffset - shadowSize + height - fuzzy;
        WindowQuad contentsQuad( mDefaultShadowQuadType, id++ );
        contentsQuad[ 0 ] = WindowVertex( x1, y1, 0, 0 );
        contentsQuad[ 1 ] = WindowVertex( x2, y1, 1, 0 );
        contentsQuad[ 2 ] = WindowVertex( x2, y2, 1, 1 );
        contentsQuad[ 3 ] = WindowVertex( x1, y2, 0, 1 );
        quadList.append( contentsQuad );
        // right
        x1 = shadowXOffset - shadowSize + width - fuzzy;
        y1 = shadowYOffset - shadowSize + 0 + fuzzy;
        x2 = shadowXOffset - shadowSize + width + fuzzy;
        y2 = shadowYOffset - shadowSize + height - fuzzy;
        WindowQuad rightQuad( mDefaultShadowQuadType, id++ );
        rightQuad[ 0 ] = WindowVertex( x1, y1, 0, 0 );
        rightQuad[ 1 ] = WindowVertex( x2, y1, 1, 0 );
        rightQuad[ 2 ] = WindowVertex( x2, y2, 1, 1 );
        rightQuad[ 3 ] = WindowVertex( x1, y2, 0, 1 );
        quadList.append( rightQuad );
        // bottom-left
        x1 = shadowXOffset - shadowSize + 0 - fuzzy;
        y1 = shadowYOffset - shadowSize + height - fuzzy;
        x2 = shadowXOffset - shadowSize + 0 + fuzzy;
        y2 = shadowYOffset - shadowSize + height + fuzzy;
        WindowQuad bottomLeftQuad( mDefaultShadowQuadType, id++ );
        bottomLeftQuad[ 0 ] = WindowVertex( x1, y1, 0, 0 );
        bottomLeftQuad[ 1 ] = WindowVertex( x2, y1, 1, 0 );
        bottomLeftQuad[ 2 ] = WindowVertex( x2, y2, 1, 1 );
        bottomLeftQuad[ 3 ] = WindowVertex( x1, y2, 0, 1 );
        quadList.append( bottomLeftQuad );
        // bottom
        x1 = shadowXOffset - shadowSize + 0 + fuzzy;
        y1 = shadowYOffset - shadowSize + height - fuzzy;
        x2 = shadowXOffset - shadowSize + width - fuzzy;
        y2 = shadowYOffset - shadowSize + height + fuzzy;
        WindowQuad bottomQuad( mDefaultShadowQuadType, id++ );
        bottomQuad[ 0 ] = WindowVertex( x1, y1, 0, 0 );
        bottomQuad[ 1 ] = WindowVertex( x2, y1, 1, 0 );
        bottomQuad[ 2 ] = WindowVertex( x2, y2, 1, 1 );
        bottomQuad[ 3 ] = WindowVertex( x1, y2, 0, 1 );
        quadList.append( bottomQuad );
        // bottom-right
        x1 = shadowXOffset - shadowSize + width - fuzzy;
        y1 = shadowYOffset - shadowSize + height - fuzzy;
        x2 = shadowXOffset - shadowSize + width + fuzzy;
        y2 = shadowYOffset - shadowSize + height + fuzzy;
        WindowQuad bottomRightQuad( mDefaultShadowQuadType, id++ );
        bottomRightQuad[ 0 ] = WindowVertex( x1, y1, 0, 0 );
        bottomRightQuad[ 1 ] = WindowVertex( x2, y1, 1, 0 );
        bottomRightQuad[ 2 ] = WindowVertex( x2, y2, 1, 1 );
        bottomRightQuad[ 3 ] = WindowVertex( x1, y2, 0, 1 );
        quadList.append( bottomRightQuad );
        } // This is called for menus, tooltips, windows where the user has disabled borders and shaped windows

    effects->buildQuads( w, quadList );
    }

QRect ShadowEffect::transformWindowDamage( EffectWindow* w, const QRect& r )
    {
    if( !useShadow( w ))
        return effects->transformWindowDamage( w, r );
    if( effects->hasDecorationShadows() )
        // TODO, HACK: We need to get the quads
        // TODO: It looks like this isn't called on resize
        return effects->transformWindowDamage( w, r.adjusted( -100, -100, 100, 100 ));
    QRect r2 = r | shadowRectangle( r );
    return effects->transformWindowDamage( w, r2 );
    }

void ShadowEffect::windowClosed( EffectWindow* c )
    {
    effects->addRepaint( shadowRectangle( c->geometry() ));
    }

bool ShadowEffect::useShadow( EffectWindow* w ) const
    {
    return !w->isDeleted() && !w->isDesktop() && !w->isDock()
        // popups may have shadow even if shaped, their shape is almost rectangular
        && ( !w->hasOwnShape() || w->isDropdownMenu() || w->isPopupMenu() || w->isComboBox());
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
        {
        // the window is additionally configured to have its opacity adjusted,
        // do it
        float opacityByBrightness = opacity * brightness;
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
        glColor4f( opacityByBrightness, opacityByBrightness, opacityByBrightness, opacity);
        }
#endif
    }

// Modified version of SceneOpenGL::Window::restoreRenderStates() from scene_opengl.cpp
void ShadowEffect::restoreRenderStates( GLTexture *texture, double opacity, double brightness, double saturation )
    {
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
        glColor4f( 0, 0, 0, 0 );
        }

    glPopAttrib();  // ENABLE_BIT
#endif
    }

void ShadowEffect::drawShadow( EffectWindow* window, int mask, QRegion region, const WindowPaintData& data )
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( effects->compositingType() == OpenGLCompositing)
        {
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

        foreach( const WindowQuad &quad, data.quads )
            {
            if( !mShadowQuadTypes.contains( quad.type() ) && quad.type() != mDefaultShadowQuadType )
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
            texcoords << quad[3].textureX() << quad[3].textureY();
            texcoords << quad[2].textureX() << quad[2].textureY();
            texcoords << quad[1].textureX() << quad[1].textureY();
            texcoords << quad[0].textureX() << quad[0].textureY();

            // Work out which texture to use
            int texture = mShadowQuadTypes.indexOf( quad.type() );
            if( texture != -1 && texture < mShadowTextures.size() ) // TODO: Needed?
                {
                // Render it!
                // Cheat a little, assume the active and inactive shadows have identical quads
                if( effects->hasDecorationShadows() )
                    {
                    if( window->hasDecoration() &&
                        effects->shadowTextureList( ShadowBorderedActive ) == texture )
                        { // Decorated windows
                        // Active shadow
                        glColor4f( 1.0, 1.0, 1.0, 1.0 );
                        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                        prepareRenderStates(
                            mShadowTextures.at( texture ).at( quad.id() ),
                            data.opacity * window->shadowOpacity( ShadowBorderedActive ),
                            data.brightness * window->shadowBrightness( ShadowBorderedActive ),
                            data.saturation * window->shadowSaturation( ShadowBorderedActive )
                            );
                        mShadowTextures.at( texture ).at( quad.id() )->bind();
                        mShadowTextures.at( texture ).at( quad.id() )->enableNormalizedTexCoords();
                        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
                        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
                        renderGLGeometry( region, 4, verts.data(), texcoords.data() );
                        mShadowTextures.at( texture ).at( quad.id() )->disableNormalizedTexCoords();
                        mShadowTextures.at( texture ).at( quad.id() )->unbind();
                        restoreRenderStates(
                            mShadowTextures.at( texture ).at( quad.id() ),
                            data.opacity * window->shadowOpacity( ShadowBorderedActive ),
                            data.brightness * window->shadowBrightness( ShadowBorderedActive ),
                            data.saturation * window->shadowSaturation( ShadowBorderedActive )
                            );

                        // Inactive shadow
                        texture = effects->shadowTextureList( ShadowBorderedInactive );

                        glColor4f( 1.0, 1.0, 1.0, 1.0 );
                        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                        prepareRenderStates(
                            mShadowTextures.at( texture ).at( quad.id() ),
                            data.opacity * window->shadowOpacity( ShadowBorderedInactive ),
                            data.brightness * window->shadowBrightness( ShadowBorderedInactive ),
                            data.saturation * window->shadowSaturation( ShadowBorderedInactive )
                            );
                        mShadowTextures.at( texture ).at( quad.id() )->bind();
                        mShadowTextures.at( texture ).at( quad.id() )->enableNormalizedTexCoords();
                        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
                        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
                        renderGLGeometry( region, 4, verts.data(), texcoords.data() );
                        mShadowTextures.at( texture ).at( quad.id() )->disableNormalizedTexCoords();
                        mShadowTextures.at( texture ).at( quad.id() )->unbind();
                        restoreRenderStates(
                            mShadowTextures.at( texture ).at( quad.id() ),
                            data.opacity * window->shadowOpacity( ShadowBorderedInactive ),
                            data.brightness * window->shadowBrightness( ShadowBorderedInactive ),
                            data.saturation * window->shadowSaturation( ShadowBorderedInactive )
                            );
                        }
                    else if( effects->shadowTextureList( ShadowBorderlessActive ) == texture )
                        { // Decoration-less normal windows
                        glColor4f( 1.0, 1.0, 1.0, 1.0 );
                        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                        if( effects->activeWindow() == window )
                            {
                            prepareRenderStates(
                                mShadowTextures.at( texture ).at( quad.id() ),
                                data.opacity * window->shadowOpacity( ShadowBorderlessActive ),
                                data.brightness * window->shadowBrightness( ShadowBorderlessActive ),
                                data.saturation * window->shadowSaturation( ShadowBorderlessActive )
                                );
                            }
                        else
                            {
                            texture = effects->shadowTextureList( ShadowBorderlessInactive );
                            prepareRenderStates(
                                mShadowTextures.at( texture ).at( quad.id() ),
                                data.opacity * window->shadowOpacity( ShadowBorderlessInactive ),
                                data.brightness * window->shadowBrightness( ShadowBorderlessInactive ),
                                data.saturation * window->shadowSaturation( ShadowBorderlessInactive )
                                );
                            }
                        mShadowTextures.at( texture ).at( quad.id() )->bind();
                        mShadowTextures.at( texture ).at( quad.id() )->enableNormalizedTexCoords();
                        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
                        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
                        renderGLGeometry( region, 4, verts.data(), texcoords.data() );
                        mShadowTextures.at( texture ).at( quad.id() )->disableNormalizedTexCoords();
                        mShadowTextures.at( texture ).at( quad.id() )->unbind();
                        if( effects->activeWindow() == window )
                            {
                            restoreRenderStates(
                                mShadowTextures.at( texture ).at( quad.id() ),
                                data.opacity * window->shadowOpacity( ShadowBorderlessActive ),
                                data.brightness * window->shadowBrightness( ShadowBorderlessActive ),
                                data.saturation * window->shadowSaturation( ShadowBorderlessActive )
                                );
                            }
                        else
                            {
                            restoreRenderStates(
                                mShadowTextures.at( texture ).at( quad.id() ),
                                data.opacity * window->shadowOpacity( ShadowBorderlessInactive ),
                                data.brightness * window->shadowBrightness( ShadowBorderlessInactive ),
                                data.saturation * window->shadowSaturation( ShadowBorderlessInactive )
                                );
                            }
                        }
                    else
                        { // Other windows
                        glColor4f( 1.0, 1.0, 1.0, 1.0 );
                        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                        prepareRenderStates(
                            mShadowTextures.at( texture ).at( quad.id() ),
                            data.opacity * window->shadowOpacity( ShadowOther ),
                            data.brightness * window->shadowBrightness( ShadowOther ),
                            data.saturation * window->shadowSaturation( ShadowOther )
                            );
                        mShadowTextures.at( texture ).at( quad.id() )->bind();
                        mShadowTextures.at( texture ).at( quad.id() )->enableNormalizedTexCoords();
                        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
                        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
                        renderGLGeometry( region, 4, verts.data(), texcoords.data() );
                        mShadowTextures.at( texture ).at( quad.id() )->disableNormalizedTexCoords();
                        mShadowTextures.at( texture ).at( quad.id() )->unbind();
                        restoreRenderStates(
                            mShadowTextures.at( texture ).at( quad.id() ),
                            data.opacity * window->shadowOpacity( ShadowOther ),
                            data.brightness * window->shadowBrightness( ShadowOther ),
                            data.saturation * window->shadowSaturation( ShadowOther )
                            );
                        }
                    }
                }
            if( quad.type() == mDefaultShadowQuadType )
                { // Default shadow
                float opacity = shadowOpacity;
                if( intensifyActiveShadow && window == effects->activeWindow() )
                    opacity = 1 - ( 1 - shadowOpacity ) * ( 1 - shadowOpacity );

                glColor4f( shadowColor.redF(), shadowColor.greenF(), shadowColor.blueF(), opacity * data.opacity );
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                prepareRenderStates(
                    mDefaultShadowTextures.at( quad.id() ),
                    data.opacity * opacity,
                    data.brightness,
                    data.saturation
                    );
                mDefaultShadowTextures.at( quad.id() )->bind();
                mDefaultShadowTextures.at( quad.id() )->enableNormalizedTexCoords();
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
                renderGLGeometry( region, 4, verts.data(), texcoords.data() );
                mDefaultShadowTextures.at( quad.id() )->disableNormalizedTexCoords();
                mDefaultShadowTextures.at( quad.id() )->unbind();
                restoreRenderStates(
                    mDefaultShadowTextures.at( quad.id() ),
                    data.opacity * opacity,
                    data.brightness,
                    data.saturation
                    );
                }

            glPopMatrix();
            }

        glPopAttrib();
        }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if( effects->compositingType() == XRenderCompositing)
        {
        // calculate opacity =================================================
        float opacity;
        if( intensifyActiveShadow && window == effects->activeWindow() )
            {
            opacity = (1 - (1 - shadowOpacity)*(1 - shadowOpacity)) * data.opacity;
            }
        else
            {
            opacity = data.opacity * shadowOpacity;
            }
            
        // query rect and translate in case (may have impact on opacity)===========================
        QRect r = window->geometry();
        if ( mask & (PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_TRANSFORMED))
            {
            float xScale = 1.0, yScale = 1.0, xTranslate = 0.0, yTranslate = 0.0;
            if ( mask & PAINT_SCREEN_TRANSFORMED)
                {
                xScale = gScreenData.xScale; yScale = gScreenData.yScale;
                xTranslate += (xScale-1.0)*r.x() + gScreenData.xTranslate;
                yTranslate += (yScale-1.0)*r.y() + gScreenData.yTranslate;
                }
                
            if ( mask & PAINT_WINDOW_TRANSFORMED)
                {
                xTranslate += xScale*data.xTranslate;
                yTranslate += yScale*data.yTranslate;
                xScale *= data.xScale; yScale *= data.yScale;
                }
                
            r.translate(xTranslate, yTranslate);
            
            if (xScale != 1.0 || yScale != 1.0)
                {
                r.setWidth(xScale * r.width());
                r.setHeight(yScale * r.height());
//                 opacity *= 2.0/(2 - (2 - (xScale + yScale))*(2 - (xScale + yScale)));
                }
            }
        r = shadowRectangle(r);

        // create render mask  ==================================================================

        XRenderColor xc = preMultiply(shadowColor, opacity);
        XRenderPicture fill = xRenderFill(&xc);

        // clip, then paint shadow tiles ========================================================
        XRenderSetPictureClipRegion (display(), effects->xrenderBufferPicture(),  region.handle());
#define DRAW_CORNER(_CNR_, _X_, _Y_)\
XRenderComposite( display(), PictOpOver, fill, mShadowPics->_CNR_, effects->xrenderBufferPicture(), 0, 0, 0, 0, _X_, _Y_, w, h )
#define DRAW_TILE(_TILE_, _X_, _Y_, _W_, _H_)\
XRenderComposite( display(), PictOpOver, fill, mShadowPics->_TILE_, effects->xrenderBufferPicture(), 0, 0, 0, 0, _X_, _Y_, _W_, _H_ )
        int w = qMin(mShadowPics->cornerSize.width(), r.width()/2);
        int h = qMin(mShadowPics->cornerSize.height(), r.height()/2);
        DRAW_CORNER(topLeft, r.x(), r.y());
        DRAW_CORNER(topRight, r.right()-w, r.y());
        DRAW_CORNER(btmLeft, r.x(), r.bottom()-h);
        DRAW_CORNER(btmRight, r.right()-w, r.bottom()-h);
        int w2 = r.width()-2*w-1, h2 = r.height()-2*h-1;
        DRAW_TILE(top, r.x()+w, r.y(), w2, h);
        DRAW_TILE(btm, r.x()+w, r.bottom()-h, w2, h);
        DRAW_TILE(left, r.x(), r.y()+h, w, h2);
        DRAW_TILE(right, r.right()-w, r.y()+h, w, h2);
        DRAW_TILE(center, r.x()+w, r.y()+h, w2, h2);
#undef DRAW_CORNER
#undef DRAW_TILE
        }
#endif
    }

} // namespace

#include "shadow.h"
