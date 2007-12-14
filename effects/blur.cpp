/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#include "blur.h"

#include <kwinglutils.h>

#include <QString>
#include <KStandardDirs>

#include <kdebug.h>


namespace KWin
{

KWIN_EFFECT( blur, BlurEffect )
KWIN_EFFECT_SUPPORTED( blur, BlurEffect::supported() )


BlurEffect::BlurEffect() : Effect()
    {
    mSceneTexture = 0;
    mTmpTexture = 0;
    mBlurTexture = 0;
    mSceneTarget = 0;
    mTmpTarget = 0;
    mBlurTarget = 0;
    mBlurShader = 0;
    mWindowShader = 0;

    mBlurRadius = 4;
    mValid = loadData();
    if( !mValid )
        {
        kWarning() << "Loading failed";
        }
    effects->addRepaintFull();
    }

BlurEffect::~BlurEffect()
    {
    effects->addRepaintFull();
    delete mSceneTexture;
    delete mTmpTexture;
    delete mBlurTexture;
    delete mSceneTarget;
    delete mTmpTarget;
    delete mBlurTarget;
    delete mBlurShader;
    delete mWindowShader;
    }


bool BlurEffect::loadData()
    {
    // Create texture and render target
    int texw = displayWidth();
    int texh = displayHeight();
    if( !GLTexture::NPOTTextureSupported() )
    {
        kWarning( 1212 ) << "NPOT textures not supported, wasting some memory" ;
        texw = nearestPowerOfTwo(texw);
        texh = nearestPowerOfTwo(texh);
    }
    mSceneTexture = new GLTexture(texw, texh);
    mSceneTexture->setFilter(GL_LINEAR);
    mTmpTexture = new GLTexture(texw, texh);
    mTmpTexture->setFilter(GL_LINEAR);
    mBlurTexture = new GLTexture(texw, texh);

    mSceneTarget = new GLRenderTarget(mSceneTexture);
    if( !mSceneTarget->valid() )
        return false;
    mTmpTarget = new GLRenderTarget(mTmpTexture);
    if( !mTmpTarget->valid() )
        return false;
    mBlurTarget = new GLRenderTarget(mBlurTexture);
    if( !mBlurTarget->valid() )
        return false;

    mBlurShader = loadShader("blur");
    if( !mBlurShader )
        return false;
    mWindowShader = loadShader("blur-render");
    if( !mWindowShader )
        return false;

    mBlurShader->bind();
    mBlurShader->setUniform("inputTex", 0);
    mBlurShader->setUniform("textureWidth", (float)texw);
    mBlurShader->setUniform("textureHeight", (float)texh);
    mBlurShader->unbind();

    mWindowShader->bind();
    mWindowShader->setUniform("windowTex", 0);
    mWindowShader->setUniform("backgroundTex", 4);
    mWindowShader->setUniform("textureWidth", (float)texw);
    mWindowShader->setUniform("textureHeight", (float)texh);
    mWindowShader->unbind();

    return true;
    }

GLShader* BlurEffect::loadShader(const QString& name)
{
    QString fragmentshader =  KGlobal::dirs()->findResource("data", "kwin/" + name + ".frag");
    QString vertexshader =  KGlobal::dirs()->findResource("data", "kwin/" + name + ".vert");
    if(fragmentshader.isEmpty() || vertexshader.isEmpty())
    {
        kError() << "Couldn't locate shader files for '" << name << "'" << endl;
        return false;
    }
    GLShader* shader = new GLShader(vertexshader, fragmentshader);
    if(!shader->isValid())
    {
        kError() << "Shader '" << name << "' failed to load!" << endl;
        delete shader;
        return 0;
    }
    return shader;
}

bool BlurEffect::supported()
    {
    return GLRenderTarget::supported() &&
            GLShader::fragmentShaderSupported() &&
            (effects->compositingType() == OpenGLCompositing);
    }

QRegion BlurEffect::expandedRegion( const QRegion& region ) const
    {
    QRegion expandedregion;
    foreach( QRect r, region.rects() )
        {
        r.adjust( -mBlurRadius, -mBlurRadius, mBlurRadius, mBlurRadius );
        expandedregion += r;
        }
    return expandedregion;
    }

void BlurEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    mTransparentWindows = 0;
    mScreenDirty = QRegion();
    mBlurDirty = QRegion();
    mBlurMask = QRegion();

    effects->prePaintScreen(data, time);
    }

void BlurEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    // Expand the painted area
    mBlurMask |= expandedRegion( data.paint );
    data.paint |= expandedRegion( mBlurMask );
    effects->prePaintWindow( w, data, time );

    if( w->isPaintingEnabled() && ( data.mask & PAINT_WINDOW_TRANSLUCENT ))
        mTransparentWindows++;
    data.setTranslucent();
    }

void BlurEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    // TODO: prePaintWindow() gets called _after_ paintScreen(), so we have no
    //  way of knowing here whether there will be any translucent windows or
    //  not. If we'd know that there's no translucent windows then we could
    //  render straight onto screen, saving some time.
    if( mValid /*&& mTransparentWindows*/ )
        {
        // rendering everything onto render target
        effects->pushRenderTarget(mSceneTarget);
        effects->paintScreen( mask, region, data );
        effects->popRenderTarget();

        // Copy changed areas back onto screen
        mScreenDirty &= mBlurMask;
        if( !mScreenDirty.isEmpty() )
            {
            if( mask & PAINT_SCREEN_TRANSFORMED )
                {
                // We don't want any transformations when working with our own
                //  textures, so load an identity matrix
                glMatrixMode( GL_MODELVIEW );
                glPushMatrix();
                glLoadIdentity();
                }

            GLTexture* tex = mSceneTexture;
            int pixels = 0;
            tex->bind();
            tex->enableUnnormalizedTexCoords();
            foreach( QRect r, mScreenDirty.rects() )
                {
                r.adjust(0, -1, 0, -1);
                int rx2 = r.x() + r.width();
                int ry2 = r.y() + r.height();
                glBegin(GL_QUADS);
                    glTexCoord2f( r.x(), ry2 );   glVertex2f( r.x(), ry2 );
                    glTexCoord2f( rx2  , ry2 );   glVertex2f( rx2  , ry2 );
                    glTexCoord2f( rx2  , r.y() ); glVertex2f( rx2  , r.y() );
                    glTexCoord2f( r.x(), r.y() ); glVertex2f( r.x(), r.y() );
                glEnd();
                pixels += r.width()*r.height();
                }
            tex->disableUnnormalizedTexCoords();
            tex->unbind();

            if( mask & PAINT_SCREEN_TRANSFORMED )
                {
                // Restore the original matrix
                glPopMatrix();
                }
//             kDebug() << "Copied" << mScreenDirty.rects().count() << "rects,    pixels:" << pixels;
            }

        }
    else
        {
        effects->paintScreen( mask, region, data );
        }
    }

void BlurEffect::drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( mValid /*&& mTransparentWindows*/ )
        {
            if( mask & PAINT_WINDOW_TRANSLUCENT &&
                (data.opacity != 1.0 || data.contents_opacity != 1.0 || data.decoration_opacity != 1.0 ))
            {
            // Make sure the blur texture is up to date
            if( mask & PAINT_SCREEN_TRANSFORMED )
                {
                // We don't want any transformations when working with our own
                //  textures, so load an identity matrix
                glPushMatrix();
                glLoadIdentity();
                }
            // If we're having transformations, we don't know the window's
            //  transformed position on the screen and thus have to update the
            //  entire screen
            if( mask & ( PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS ) )
                updateBlurTexture( QRegion(0, 0, displayWidth(), displayHeight()) );
            else
                updateBlurTexture( mBlurDirty );
            mBlurDirty = QRegion();
            if( mask & PAINT_SCREEN_TRANSFORMED )
                // Restore the original matrix
                glPopMatrix();

            // Set custom shader to render the window with
            mWindowShader->bind();
            data.shader = mWindowShader;
            // Put the blur texture to tex unit 4
            glActiveTexture(GL_TEXTURE4);
            mBlurTexture->bind();
            glActiveTexture(GL_TEXTURE0);

            // Paint
            effects->drawWindow( w, mask, region, data );

            // Disable blur texture and shader
            glActiveTexture(GL_TEXTURE4);
            mBlurTexture->unbind();
            glActiveTexture(GL_TEXTURE0);
            mWindowShader->unbind();
            }
        else
            {
            // Opaque window
            // Paint to the rendertarget (which is already being used)
            effects->drawWindow( w, mask, region, data );
            }
        // Mark the window's region as dirty
        mScreenDirty += region;
        mBlurDirty += region & mBlurMask;
        }
    else
        // If there are no translucent windows then paint as usual
        effects->drawWindow( w, mask, region, data );
    }

void BlurEffect::updateBlurTexture(const QRegion& region)
    {
    QRect bounding = region.boundingRect();
    QVector<QRect> rects = region.rects();
    int totalarea = 0;
    foreach( QRect r, rects )
        totalarea += r.width() * r.height();
    if( (int)(totalarea * 1.33 + 100 ) < bounding.width() * bounding.height() )
        {
        // Use small rects
        updateBlurTexture(rects);
        }
    else
        {
        // Bounding rect is probably cheaper
        QVector<QRect> tmp( 1, bounding );
        updateBlurTexture( tmp );
        }
    }

void BlurEffect::updateBlurTexture(const QVector<QRect>& rects)
    {
    // Blur
    // First pass (vertical)
    mBlurShader->bind();
    effects->pushRenderTarget(mTmpTarget);
    mBlurShader->setAttribute("xBlur", 0.0f);
    mBlurShader->setAttribute("yBlur", 1.0f);

    mSceneTexture->bind();

    foreach( QRect r, rects )
        {
        // We change x coordinates here because horizontal blur pass (which
        //  comes after this one) also uses pixels that are horizontally edging
        //  the blurred area. Thus we need to make sure that those pixels are
        //  also updated.
        glBegin(GL_QUADS);
            glVertex2f( r.x()-mBlurRadius            , r.y() + r.height() );
            glVertex2f( r.x() + r.width()+mBlurRadius, r.y() + r.height() );
            glVertex2f( r.x() + r.width()+mBlurRadius, r.y() );
            glVertex2f( r.x()-mBlurRadius            , r.y() );
        glEnd();
        }


    mSceneTexture->unbind();
    effects->popRenderTarget();

    // Second pass (horizontal)
    effects->pushRenderTarget(mBlurTarget);
    mBlurShader->setAttribute("xBlur", 1.0f);
    mBlurShader->setAttribute("yBlur", 0.0f);

    mTmpTexture->bind();

    foreach( QRect r, rects )
        {
        glBegin(GL_QUADS);
            glVertex2f( r.x()            , r.y() + r.height() );
            glVertex2f( r.x() + r.width(), r.y() + r.height() );
            glVertex2f( r.x() + r.width(), r.y() );
            glVertex2f( r.x()            , r.y() );
        glEnd();
        }


    mTmpTexture->unbind();
    effects->popRenderTarget();
    mBlurShader->unbind();
    }

} // namespace

