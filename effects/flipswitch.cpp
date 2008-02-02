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
#include "flipswitch.h"

#include <kwinconfig.h>
#include <QApplication>
#include <QDesktopWidget>
#include <QFont>
#include <kapplication.h>
#include <kcolorscheme.h>
#include <kconfiggroup.h>

#include <kwinglutils.h>

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
#include <GL/gl.h>
#endif

namespace KWin
{

KWIN_EFFECT( flipswitch, FlipSwitchEffect )

FlipSwitchEffect::FlipSwitchEffect()
    : mActivated( 0 )
    , animateFlip( false )
    , forward( true )
    , start( false )
    , stop( false )
    , addFullRepaint( false )
    , rearrangeWindows( 0 )
    , stopRequested( false )
    , startRequested( false )
    {
    KConfigGroup conf = effects->effectConfig( "FlipSwitch" );
    mFlipDuration = conf.readEntry( "FlipDuration", 300 );
    mAnimation    = conf.readEntry( "AnimateFlip", true );
    }

FlipSwitchEffect::~FlipSwitchEffect()
    {
    }

void FlipSwitchEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( mActivated || stopRequested || stop )
        {
         data.mask |= Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        }
    effects->prePaintScreen(data, time);
    }

void FlipSwitchEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
    if( mActivated || stopRequested || stop )
        {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
        glMatrixMode( GL_PROJECTION );
        glPushMatrix();
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable( GL_DEPTH_TEST );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        glEnable( GL_POLYGON_SMOOTH );
        glLoadIdentity();
        glFrustum(-QApplication::desktop()->geometry().width()*0.5f,
            QApplication::desktop()->geometry().width()*0.5f,
            QApplication::desktop()->geometry().height()*0.5f,
            -QApplication::desktop()->geometry().height()*0.5f, 10, 50);

        glMatrixMode( GL_MODELVIEW );
        glLoadIdentity();
        float xOffset = QApplication::desktop()->geometry().width()*0.33f;
        float zOffset = 10.0;

        // bring the selected window to the back of the list
        QList< EffectWindow* > tempList = effects->currentTabBoxWindowList();
        int index = tempList.indexOf( effects->currentTabBoxWindow() );
        QList< EffectWindow* > windowList;
        for( int i=index; i<tempList.count(); i++)
            {
            windowList.push_front( tempList[i] );
            }
        for( int i=0; i<index; i++)
            {
            windowList.push_front( tempList[i] );
            }
        // do we have to rearrange the windows as an animation has to follow?
        if( rearrangeWindows != 0 )
            {
            if( rearrangeWindows < 0 )
                {
                for( int i=0; i>rearrangeWindows; i-- )
                    {
                    EffectWindow* w = windowList.front();
                    windowList.pop_front();
                    windowList.append( w );
                    }
                }
            else
                {
                for( int i=0; i<rearrangeWindows; i++ )
                    {
                    EffectWindow* w = windowList.back();
                    windowList.pop_back();
                    windowList.prepend( w );
                    }
                }
            }

        // do we have a start or stop animation
        if( ( start || stop ) && mAnimation )
            {
            int elapsed = animationTime.elapsed();
            for( int i=0; i<windowList.count(); i++)
                {
                glPushMatrix();
                EffectWindow *w = windowList[ i ];        
                // Position of the window in OpenGL
                float x = w->x()-QApplication::desktop()->geometry().width()*0.5f;
                float y = -QApplication::desktop()->geometry().height()*1.5f+w->y()+w->height();
                float z = -10.0;
                if( w->isMinimized() )
                    {
                    // use icon instead of window
                    x = w->iconGeometry().x()-QApplication::desktop()->geometry().width()*0.5f;
                    y = -QApplication::desktop()->geometry().height()*1.5f+w->iconGeometry().y()+w->height();
                    }
                // Position of the window in the stack
                float stackX = -QApplication::desktop()->geometry().width()*0.25f-(xOffset*windowList.count())+xOffset*(i+1);
                float stackY = -QApplication::desktop()->geometry().height()*0.5f;
                float stackZ = (-1*zOffset*windowList.count()) -12.5+zOffset*(i+1);

                float timeFactor = (float)((float)elapsed/(float)mFlipDuration);
                // limit animation to final position
                if( timeFactor > 1.0 ) timeFactor = 1.0;

                float animateXOffset;
                float animateYOffset;
                float animateZOffset;
                float rotation;
                // if start move to stack, if stop move from stack
                if( start )
                    {
                    animateXOffset = x+timeFactor*(stackX-x);
                    animateYOffset = y+timeFactor*(stackY-y);
                    animateZOffset = z+timeFactor*(stackZ-z);
                    rotation = timeFactor*0.25;
                    }
                else if( stop )
                    {
                    animateXOffset = stackX+timeFactor*(x-stackX);
                    animateYOffset = stackY+timeFactor*(y-stackY);
                    animateZOffset = stackZ+timeFactor*(z-stackZ);
                    rotation = 0.25-timeFactor*0.25;
                    }

                // go to current position and rotate by the time based factor
                glTranslatef(animateXOffset, animateYOffset, animateZOffset );
                glRotatef(rotation, 0.0, 1.0, 0.0);

                // top most window has to be painted not drawn.
                if( i<windowList.count()-1 )
                    {
                    paintWindowFlip( windowList[i] );
                    }
                else paintWindowFlip( windowList[i], false);
                glPopMatrix();
                }
            // time elapsed - so no more animation
            if( elapsed >= mFlipDuration )
                {
                if( start )
                    {
                    start = false;
                    // more animations have to follow?
                    if( rearrangeWindows != 0 )
                        {
                        animateFlip = true;
                        animationTime.restart();
                        if( rearrangeWindows < 0 )
                            {
                            forward = true;
                            rearrangeWindows++;
                            }
                        else
                            {
                            forward = false;
                            rearrangeWindows--;
                            }
                        }
                    else if( stopRequested )
                        {
                        // no more animations but effect has to stop
                        stop = true;
                        stopRequested = false;
                        animationTime.restart();
                        }
                    }
                else if( stop )
                    {
                    stop = false;
                    if( startRequested )
                        {
                        // tabbox allready referenced again - so restart
                        start = true;
                        startRequested = false;
                        mActivated = true;
                        animationTime.restart();
                        }
                    else
                        {
                        mActivated = false;
                        // we need one more FullRepaint
                        addFullRepaint = true;
                        }
                    }
                }
            }
        // normal behaviour - no start or stop animation
        else
            {
            glPushMatrix();
            glTranslatef(-QApplication::desktop()->geometry().width()*0.25f-(xOffset*windowList.count()),
                -QApplication::desktop()->geometry().height()*0.5f,
                (-1*zOffset*windowList.count()) -12.5);
            int elapsed = animationTime.elapsed();
            float timeFactor = (float)((float)elapsed/(float)mFlipDuration);
            if( animateFlip && windowList.count() > 1 )
                {
                if( elapsed <= mFlipDuration)
                    {
                    float animateXOffset = timeFactor*xOffset;
                    float animateZOffset = timeFactor*zOffset;
                    if( forward )
                        {
                        if( animateXOffset > xOffset ) animateXOffset = xOffset;
                        if( animateZOffset > zOffset ) animateZOffset = zOffset;
                        glTranslatef(animateXOffset, 0.0, animateZOffset);
                        EffectWindow* w = windowList.front();
                        windowList.pop_front();
                        windowList.append( w );
                        }
                    else
                        {
                        animateXOffset = xOffset - animateXOffset;
                        animateZOffset = zOffset - animateZOffset;
                        if( animateXOffset < 0.0 ) animateXOffset = 0.0;
                        if( animateZOffset < 0.0 ) animateZOffset = 0.0;
                        glTranslatef(animateXOffset, 0.0, animateZOffset);
                        }
                    }
                else
                    {
                    if( rearrangeWindows != 0 )
                        {
                        animateFlip = true;
                        animationTime.restart();
                        if( rearrangeWindows < 0 )
                            {
                            forward = true;
                            rearrangeWindows++;
                            timeFactor = 0.0;
                            }
                        else
                            {
                            forward = false;
                            rearrangeWindows--;
                            timeFactor = 1.0;
                            }
                        }
                    else
                        {
                        animateFlip = false;
                        if( stopRequested )
                            {
                            stop = true;
                            stopRequested = false;
                            animationTime.restart();
                            }
                        }
                    }
                }
            for( int i=0; i<windowList.count(); i++)
                {
                glPushMatrix();
                glTranslatef(xOffset*(i+1), 0.0, zOffset*(i+1));
                glRotatef(0.25, 0.0, 1.0, 0.0);
                // top most window has to be painted not drawn.
                if( i<windowList.count()-1 )
                    {
                    paintWindowFlip( windowList[i] );
                    }
                else
                    {
                    // last window - change opacity if animated
                    float opacity = 0.8;
                    if( animateFlip && windowList.count() > 1 )
                        {
                        if( forward ) opacity = opacity - timeFactor*opacity;
                        else opacity = timeFactor*opacity;
                        }
                    paintWindowFlip( windowList[i], false, opacity);
                    }
                glPopMatrix();
                }
            glPopMatrix();
            }
        glPopAttrib();
        glMatrixMode( GL_PROJECTION );
        glPopMatrix();
        glMatrixMode( GL_MODELVIEW );

        // caption of selected window
        QColor color_frame;
        QColor color_text;
        color_frame = KColorScheme( QPalette::Active, KColorScheme::Window ).background().color();
        color_frame.setAlphaF( 0.9 );
        color_text = KColorScheme( QPalette::Active, KColorScheme::Window ).foreground().color();
        QFont text_font;
        text_font.setBold( true );
        text_font.setPointSize( 20 );
        glPushAttrib( GL_CURRENT_BIT );
        glColor4f( color_frame.redF(), color_frame.greenF(), color_frame.blueF(), color_frame.alphaF());
        QRect frameRect = QRect( QApplication::desktop()->geometry().width()*0.25f,
            QApplication::desktop()->geometry().height()*0.9f,
            QApplication::desktop()->geometry().width()*0.5f,
            QFontMetrics( text_font ).height() * 1.2 );
        renderRoundBoxWithEdge( frameRect );
        effects->paintText( effects->currentTabBoxWindow()->caption(),
            frameRect.center(),
            frameRect.width() - 100,
            color_text,
            text_font );
        glPopAttrib();
        // icon of selected window
        GLTexture* icon = new GLTexture( effects->currentTabBoxWindow()->icon() );
        icon->bind();
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
        icon->bind();
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        // icon takes 80 % of the height of the frame. So each 10 % space left on the top and botton
        QRect iconRect = QRect( frameRect.x() + frameRect.height()*0.1f,
            frameRect.y() + frameRect.height()*0.1f,
            frameRect.height()*0.8f,
            frameRect.height()*0.8f );
        icon->render( false, region, iconRect);
        icon->unbind();
        glPopAttrib();
#endif
        }
    }

void FlipSwitchEffect::postPaintScreen()
    {
    if( (mActivated && ( animateFlip || start )) || stopRequested || stop )
        {
        effects->addRepaintFull();
        }
    if( addFullRepaint )
        {
        addFullRepaint = false;
        effects->addRepaintFull();
        }
    effects->postPaintScreen();
    }
  
void FlipSwitchEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( mActivated || stopRequested || stop )
        {
        if( !( mask & PAINT_WINDOW_TRANSFORMED ) && ( !w->isDesktop() ) )
            {
            return;
            }
        }
    effects->paintWindow( w, mask, region, data );
    }
  
void FlipSwitchEffect::tabBoxAdded( int mode )
    {
    if( !mActivated )
        {
        // only for windows mode
        if( mode == TabBoxWindowsMode && effects->currentTabBoxWindowList().count() > 0 )
            {
            effects->refTabBox();
            selectedWindow = effects->currentTabBoxWindowList().indexOf(effects->currentTabBoxWindow());
            if( !stop && !stopRequested )
                {
                mActivated = true;
                start = true;
                animationTime.restart();
                effects->addRepaintFull();
                }
            else
                {
                // last tabbox effect still running - schedule start effect
                startRequested = true;
                }
            }
        }
    }
void FlipSwitchEffect::tabBoxClosed()
    {
    if( mActivated )
        {
        // if animation than deactivate after animation
        mActivated = false;
        effects->unrefTabBox();
        if( mAnimation )
            {
            if ( start || animateFlip ) stopRequested = true;
            else
                {
                stop = true;
                animationTime.restart();
                effects->addRepaintFull();
                }
            }
        }
    }

void FlipSwitchEffect::tabBoxUpdated()
    {
    if( mActivated )
        {
        if( mAnimation )
            {
            // determine the switch direction
            int index = effects->currentTabBoxWindowList().indexOf(effects->currentTabBoxWindow());
            bool direction = false;
            int windowCount = effects->currentTabBoxWindowList().count();
            if( index > selectedWindow )
                {
                if( index == windowCount-1 && selectedWindow == 0 ) forward = false;
                else direction = true;
                }
            else if( index == 0 && ( selectedWindow == windowCount-1 ) )
                {
                direction = true;
                }
            else if( index == selectedWindow ) return; // nothing changed
            else
                {
                direction = false;
                }
            selectedWindow = index;
            if( !animateFlip && !start )
                {
                forward = direction;
                animateFlip = true;
                animationTime.restart();
                }
            else
                {
                if( direction ) rearrangeWindows--;
                else rearrangeWindows++;
                if( rearrangeWindows >= windowCount ) rearrangeWindows = rearrangeWindows % windowCount;
                else if( (-1*rearrangeWindows) >= windowCount ) rearrangeWindows = -1*((-1*rearrangeWindows) % windowCount);
                }
            }
        effects->addRepaintFull();
        }
    }

void FlipSwitchEffect::paintWindowFlip( EffectWindow* w, bool draw, float opacity )
    {
    WindowPaintData data( w );

    int x = 0;
    int y = QApplication::desktop()->geometry().height() - w->geometry().height();
    QRect thumbnail;
    setPositionTransformations( data,                                             
        thumbnail, w,
        QRect( x, y, w->geometry().width(), w->geometry().height() ),
        Qt::KeepAspectRatio );

    data.opacity = opacity;
    // if paintWindow() is used the window behind the initial selected window is not painted on the stack,
    // but painted when it is selected
    // if drawWindow() is used the front window does not glide through the monitor during animation
    // so use drawWindow() for all windows but the selected and paintWindow() for the selected window
    if( draw )
        effects->drawWindow( w,
            PAINT_WINDOW_TRANSFORMED,
            thumbnail, data );
    else 
        effects->paintWindow( w,
            PAINT_WINDOW_TRANSFORMED,
            thumbnail, data );
    }

} // namespace
