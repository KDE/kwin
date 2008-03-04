/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com

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
#include "dimscreen.h"

#include <kwinglutils.h>

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
#include <GL/gl.h>
#endif

namespace KWin
{

KWIN_EFFECT( dimscreen, DimScreenEffect )

DimScreenEffect::DimScreenEffect()
    : mActivated( false )
    , animationDuration( 300 )
    , animation( false )
    , deactivate( false )
    {
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    alphaFormat = XRenderFindStandardFormat( display(), PictStandardARGB32 );
#endif
    }

DimScreenEffect::~DimScreenEffect()
    {
    }

void DimScreenEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    effects->prePaintScreen( data, time );
    }

void DimScreenEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
    if( mActivated || deactivate )
        {
        float opacity = 0.4;
        int height = Effect::displayHeight();
        if( animation )
            {
            int elapsed = animationTime.elapsed();
            float timeFactor = (float)((float)elapsed/(float)animationDuration);
            if( timeFactor > 1.0 )
                timeFactor = 1.0;
            if( deactivate )
                {
                opacity = opacity - opacity * timeFactor;
                }
            else
                {
                opacity = opacity * timeFactor;
                }
            }
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
        if( effects->compositingType() == OpenGLCompositing )
            {
            glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
            glEnable( GL_BLEND );
            glBlendFunc( GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA );
            glPolygonMode( GL_FRONT, GL_FILL );
            glColor4f( 0.0, 0.0, 0.0, opacity );
            float vertices[] = { 0.0, 0.0, 0.0, height, Effect::displayWidth(), height, Effect::displayWidth(), 0.0 };
            renderGLGeometry( 4, vertices );
            glDisable( GL_BLEND );
            glPopAttrib();
            }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        if( effects->compositingType() == XRenderCompositing )
            {
            Pixmap pixmap = XCreatePixmap( display(), rootWindow(),
                Effect::displayWidth(), Effect::displayHeight(), 32 );
            Picture pic = XRenderCreatePicture( display(), pixmap, alphaFormat, 0, NULL );
            XFreePixmap( display(), pixmap );
            XRenderColor col;
            col.alpha = int( opacity * 0xffff );
            col.red = int( 0.0 * opacity * 0xffff );
            col.green = int( 0.0 * opacity * 0xffff );
            col.blue = int( 0.0 * opacity * 0xffff );
            XRenderFillRectangle( display(), PictOpSrc, pic, &col, 0, 0,
                Effect::displayWidth(), height );
            XRenderComposite( display(), PictOpOver,
                pic, None, effects->xrenderBufferPicture(),
                0, 0, 0, 0, 0, 0, Effect::displayWidth(), height );
            XRenderFreePicture( display(), pic );
            }
#endif
        // re-paint active window
        EffectWindow* activeWindow = effects->activeWindow();
        if( activeWindow )
            {
            WindowPaintData data( activeWindow );
            effects->drawWindow( activeWindow, 0, activeWindow->geometry(), data );
            }
        }
    }

void DimScreenEffect::postPaintScreen()
    {
    if( animation )
        {
        if( animationTime.elapsed() >= animationDuration )
            {
            animation = false;
            deactivate = false;
            }
        effects->addRepaintFull();
        }
    effects->postPaintScreen();
    }

void DimScreenEffect::paintWindow( EffectWindow *w, int mask, QRegion region, WindowPaintData &data )
    {
    if( mActivated && ( w == effects->activeWindow() ) )
        {
        return;
        }
    effects->paintWindow( w, mask, region, data );
    }

void DimScreenEffect::windowActivated( EffectWindow *w )
    {
    if( !w ) return;
    QStringList check;
    check << "kdesu kdesu";
    check << "kdesudo kdesudo";
    bool before = mActivated;
    if( check.contains( w->windowClass() ) )
        {
        mActivated = true;
        }
    else
        {
        mActivated = false;
        }
    if( before != mActivated )
        {
        if( !mActivated )
            {
            deactivate = true;
            }
        animation = true;
        animationTime.restart();
        effects->addRepaintFull();
        }
    }
} // namespace
