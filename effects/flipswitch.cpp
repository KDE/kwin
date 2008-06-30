/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

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
#include "flipswitch.h"

#include <kwinconfig.h>
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
    , twinview( false )
    {
    KConfigGroup conf = effects->effectConfig( "FlipSwitch" );
    mFlipDuration = conf.readEntry( "FlipDuration", 200 );
    mAnimation    = conf.readEntry( "AnimateFlip", true );
    timeLine.setCurveShape( TimeLine::EaseInOutCurve );
    timeLine.setDuration( mFlipDuration );
    }

FlipSwitchEffect::~FlipSwitchEffect()
    {
    }

void FlipSwitchEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( mActivated || stopRequested || stop )
        {
         data.mask |= Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        if( mAnimation && ( start || stop || animateFlip ) )
            timeLine.addTime( (double)time );
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
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        glLoadIdentity();
        int viewport[4];
        glGetIntegerv( GL_VIEWPORT, viewport );
        int yPos = area.y();
        QRect fullArea = effects->clientArea( FullArea, effects->activeScreen(), effects->currentDesktop());
        if( twinview )
            {
            if( effects->clientArea( FullScreenArea, effects->activeScreen(), effects->currentDesktop()).x() == 0 
                && effects->clientArea( FullScreenArea, effects->activeScreen()==0?1:0, effects->currentDesktop()).x() == 0 )
                {
                // top <-> bottom
                // have to correct the yPos for top bottom constellation
                yPos = area.height()-area.y();
                if( ( area.height() * 2 != fullArea.height() ) ||
                    ( effects->clientArea( FullScreenArea, effects->activeScreen(), effects->currentDesktop()).width() !=  
                    effects->clientArea( FullScreenArea, effects->activeScreen()==0?1:0, effects->currentDesktop()).width() ) )
                    {
                    // different resolutions
                    if( area.y() > 0 )
                        yPos = 0;
                    else
                        yPos = fullArea.height() - area.height();
                    }
                }
            else
                {
                // left <-> right
                if( ( area.width() * 2 != fullArea.width() ) ||
                    ( effects->clientArea( FullScreenArea, effects->activeScreen(), effects->currentDesktop()).height() !=  
                    effects->clientArea( FullScreenArea, effects->activeScreen()==0?1:0, effects->currentDesktop()).height() ) )
                    {
                    // different resolutions
                    yPos = area.y() + fullArea.height() - area.height();
                    }
                }
            }
        float left, right, top, bottom;
        left = -area.width() * 0.5f;
        right = area.width() * 0.5f;
        top = area.height()*0.5f;
        bottom = -area.height()*0.5f;
        if( twinview && ( start || stop ) )
            {
            // special handling for start and stop animation in twin view setup
            glViewport( fullArea.x(), fullArea.y(), fullArea.width(), fullArea.height() );
            left = -(area.x() + area.width() * 0.5f);
            right = fullArea.width() + left;
            bottom = -(area.y() + area.height() * 0.5f);
            top = fullArea.height() + bottom;
            }
        else
            {
            glViewport( area.x(), yPos, area.width(), area.height() );
            }
        glFrustum( left, right, top, bottom, 10, 50 );

        glMatrixMode( GL_MODELVIEW );
        glLoadIdentity();
        float xOffset = area.width()*0.33f;
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
            for( int i=0; i<windowList.count(); i++)
                {
                glPushMatrix();
                EffectWindow *w = windowList[ i ];        
                // Position of the window in OpenGL
                float x = w->x()+left;
                float y = bottom-area.height()+w->y()+w->height();
                float z = -10.0;
                if( w->isMinimized() )
                    {
                    // use icon instead of window
                    x = w->iconGeometry().x()+left;
                    y = bottom-area.height()+w->iconGeometry().y()+w->height();
                    }
                // Position of the window in the stack
                float stackX = -area.width()*0.25f-(xOffset*windowList.count())+xOffset*(i+1);
                float stackY = -area.height()*0.5f;
                float stackZ = (-1*zOffset*windowList.count()) -12.5+zOffset*(i+1);

                float animateXOffset;
                float animateYOffset;
                float animateZOffset;
                float rotation;
                // if start move to stack, if stop move from stack
                if( start )
                    {
                    animateXOffset = x+timeLine.value()*(stackX-x);
                    animateYOffset = y+timeLine.value()*(stackY-y);
                    animateZOffset = z+timeLine.value()*(stackZ-z);
                    rotation = timeLine.value()*0.25;
                    }
                else // = if( stop )
                    {
                    animateXOffset = stackX+timeLine.value()*(x-stackX);
                    animateYOffset = stackY+timeLine.value()*(y-stackY);
                    animateZOffset = stackZ+timeLine.value()*(z-stackZ);
                    rotation = 0.25-timeLine.value()*0.25;
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
            if( timeLine.value() == 1.0 )
                {
                timeLine.setProgress( 0.0 );
                if( start )
                    {
                    start = false;
                    // more animations have to follow?
                    if( rearrangeWindows != 0 )
                        {
                        animateFlip = true;
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
            double localProgress = timeLine.value();
            glPushMatrix();
            glTranslatef(-area.width()*0.25f-(xOffset*windowList.count()),
                -area.height()*0.5f,
                (-1*zOffset*windowList.count()) -12.5);
            if( animateFlip && windowList.count() > 1 )
                {
                if( timeLine.value() < 1.0 )
                    {
                    float animateXOffset = timeLine.value()*xOffset;
                    float animateZOffset = timeLine.value()*zOffset;
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
                    timeLine.setProgress( 0.0 );
                    if( rearrangeWindows != 0 )
                        {
                        animateFlip = true;
                        if( rearrangeWindows < 0 )
                            {
                            forward = true;
                            rearrangeWindows++;
                            localProgress = 0.0;
                            }
                        else
                            {
                            forward = false;
                            rearrangeWindows--;
                            localProgress = 1.0;
                            }
                        }
                    else
                        {
                        animateFlip = false;
                        if( stopRequested )
                            {
                            stop = true;
                            stopRequested = false;
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
                        if( forward ) opacity = opacity - localProgress*opacity;
                        else opacity = localProgress*opacity;
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
        glViewport( viewport[0], viewport[1], viewport[2], viewport[3] );

        // caption of selected window
        QColor color_frame;
        QColor color_text;
        color_frame = KColorScheme( QPalette::Active, KColorScheme::Window ).background().color();
        color_frame.setAlphaF( 0.9 );
        color_text = KColorScheme( QPalette::Active, KColorScheme::Window ).foreground().color();
        if( start )
            {
            color_frame.setAlphaF( 0.9 * timeLine.value() );
            color_text.setAlphaF( timeLine.value() );
            }
        else if( stop )
            {
            color_frame.setAlphaF( 0.9 - 0.9 * timeLine.value() );
            color_text.setAlphaF( 1.0 - timeLine.value() );
            }
        else if( addFullRepaint )
            {
            // special case: timeLine was reset, but actual frame will be painted
            color_frame.setAlphaF( 0.0 );
            color_text.setAlphaF( 0.0 );
            }
        QFont text_font;
        text_font.setBold( true );
        text_font.setPointSize( 20 );
        glPushAttrib( GL_CURRENT_BIT );
        glColor4f( color_frame.redF(), color_frame.greenF(), color_frame.blueF(), color_frame.alphaF());
        QRect frameRect = QRect( area.width()*0.25f + area.x(),
            area.height()*0.9f + area.y(),
            area.width()*0.5f,
            QFontMetrics( text_font ).height() * 1.2 );
        renderRoundBoxWithEdge( frameRect );
        effects->paintText( effects->currentTabBoxWindow()->caption(),
            frameRect.center(),
            frameRect.width() - 100,
            color_text,
            text_font );
        glPopAttrib();
        // icon of selected window
        QPixmap iconPixmap = effects->currentTabBoxWindow()->icon();
        if( start || stop || addFullRepaint )
            {
            int alpha = 255.0f * timeLine.value();
            if( stop )
                {
                alpha = 255.0f - alpha;
                }
            else if( addFullRepaint )
                {
                alpha = 0.0f;
                }
            QPixmap transparency = iconPixmap.copy( iconPixmap.rect() );
            transparency.fill( QColor( 255, 255, 255, alpha ) );
            iconPixmap.setAlphaChannel( transparency.alphaChannel()  );
            }
        GLTexture* icon = new GLTexture( iconPixmap );
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
        icon->render( region, iconRect);
        icon->unbind();
        delete icon;
        glDisable( GL_BLEND );
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
        effects->setActiveFullScreenEffect( 0 );
        effects->destroyInputWindow( input );
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
            if( ( start || stop ) && w->isDock() )
                {
                data.opacity = 1.0 - timeLine.value();
                if( stop )
                    data.opacity = timeLine.value();
                }
            else
                return;
            }
        }
    effects->paintWindow( w, mask, region, data );
    }
  
void FlipSwitchEffect::tabBoxAdded( int mode )
    {
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return;
    if( !mActivated )
        {
        // only for windows mode
        if( mode == TabBoxWindowsMode && effects->currentTabBoxWindowList().count() > 0 )
            {
            effects->refTabBox();
            effects->setActiveFullScreenEffect( this );
            input = effects->createFullScreenInputWindow( this, Qt::BlankCursor );
            selectedWindow = effects->currentTabBoxWindowList().indexOf(effects->currentTabBoxWindow());
            if( !stop && !stopRequested )
                {
                mActivated = true;
                start = true;
                effects->addRepaintFull();
                }
            else
                {
                // last tabbox effect still running - schedule start effect
                startRequested = true;
                }
                
            // Calculation of correct area
            area = effects->clientArea( FullScreenArea, effects->activeScreen(), effects->currentDesktop());
            QRect fullArea = effects->clientArea( FullArea, effects->activeScreen(), effects->currentDesktop());
            // twinview?
            if( area.height() != fullArea.height() || area.width() != fullArea.width() )
                twinview = true;
            else
                twinview = false;
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
            if( start && rearrangeWindows == 0 )
                {
                stop = true;
                start = false;
                timeLine.setProgress( 1.0 - timeLine.value() );
                }
            else if( start || animateFlip )
                stopRequested = true;
            else
                {
                stop = true;
                effects->addRepaintFull();
                }
            }
        else
            {
            effects->setActiveFullScreenEffect( 0 );            
            effects->destroyInputWindow( input );
            }
        }
    }

void FlipSwitchEffect::tabBoxUpdated()
    {
    if( mActivated )
        {
        if( mAnimation && effects->currentTabBoxWindowList().count() > 1 )
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
    int y = area.height() - w->geometry().height();
    QRect thumbnail;
    setPositionTransformations( data,                                             
        thumbnail, w,
        QRect( x, y, w->geometry().width(), w->geometry().height() ),
        Qt::KeepAspectRatio );

    data.opacity = opacity;
    thumbnail = infiniteRegion();
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
