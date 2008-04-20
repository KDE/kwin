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
#include "coverswitch.h"

#include <kwinconfig.h>
#include <QFont>
#include <QBitmap>
#include <klocale.h>
#include <kapplication.h>
#include <kcolorscheme.h>
#include <kconfiggroup.h>

#include <kwinglutils.h>

#include <math.h>

#include <kdebug.h>

#include <GL/gl.h>

namespace KWin
{

KWIN_EFFECT( coverswitch, CoverSwitchEffect )

CoverSwitchEffect::CoverSwitchEffect()
    : mActivated( 0 )
    , angle( 60.0 )
    , animation( false )
    , start( false )
    , stop( false )
    , forward( true )
    , rearrangeWindows( 0 )
    , stopRequested( false )
    , startRequested( false )
    , twinview( false )
    {
    KConfigGroup conf = effects->effectConfig( "CoverSwitch" );
    animationDuration = conf.readEntry( "Duration", 200 );
    animateSwitch     = conf.readEntry( "AnimateSwitch", true );
    animateStart      = conf.readEntry( "AnimateStart", true );
    animateStop       = conf.readEntry( "AnimateStop", true );
    reflection        = conf.readEntry( "Reflection", true );
    }

CoverSwitchEffect::~CoverSwitchEffect()
    {
    }

void CoverSwitchEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( mActivated || stop || stopRequested )
        {
        data.mask |= Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        if( animation || start || stop )
            {
            double progress = qMin( 1.0, timeLine.value() + time / double( animationDuration ));
            timeLine.setProgress(progress);
            timeLine.setCurveShape(TimeLine::EaseInOutCurve);
            }
        }
    effects->prePaintScreen(data, time);
    }

void CoverSwitchEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );

    if( mActivated || stop || stopRequested )
        {
        glMatrixMode( GL_PROJECTION );
        glPushMatrix();
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
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
        glPushMatrix();
        glTranslatef(left, bottom, -7.5);
    
        QList< EffectWindow* > tempList = effects->currentTabBoxWindowList();
        int index = tempList.indexOf( effects->currentTabBoxWindow() );
        if( animation || start || stop )
            {
            if( !start && !stop )
                {
                if( forward )
                    index--;
                else
                    index++;
                }
            index += rearrangeWindows;
            if( index < 0 )
                index = tempList.count() + index;
            if( index >= tempList.count() )
                index = index % tempList.count();
            }
        int rightIndex = index -1;
        if( rightIndex < 0 )
            rightIndex = tempList.count() -1;
        int leftIndex = index +1;
        if( leftIndex == tempList.count() )
            leftIndex = 0;
        
        EffectWindow* frontWindow = tempList[ index ];
        QList< EffectWindow* > leftWindows;
        QList< EffectWindow* > rightWindows;
    
        bool evenWindows = ( tempList.count() % 2 == 0 ) ? true : false;
        int leftWindowCount = 0;
        if( evenWindows )
            leftWindowCount = tempList.count()/2;
        else
            leftWindowCount = ( tempList.count() - 1 )/2;
        for( int i=0; i < leftWindowCount; i++ )
            {
            int tempIndex = ( leftIndex + i ) % tempList.count();
            leftWindows.prepend( tempList[ tempIndex ] );
            }
        int rightWindowCount = 0;
        if( evenWindows )
            rightWindowCount = tempList.count()/2 - 1;
        else
            rightWindowCount = ( tempList.count() - 1 )/2;
        for( int i=0; i < rightWindowCount; i++ )
            {
            int tempIndex = ( rightIndex - i );
            if( tempIndex < 0 )
                tempIndex = tempList.count() + tempIndex;
            rightWindows.prepend( tempList[ tempIndex ] );
            }

        if( reflection )
            {
            // no reflections during start and stop animation
            if( !start && !stop )
                paintScene( frontWindow, &leftWindows, &rightWindows, true );
            glEnable( GL_BLEND );
            glBlendFunc( GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA );
            glPolygonMode( GL_FRONT, GL_FILL );
            glPushMatrix();
            float leftVertex = 0.0;
            if( twinview && ( start || stop ) )
                {
                leftVertex = area.x();
                glTranslatef( 0.0, area.y() + area.height(), 7.5);
                }
            else
                {
                glTranslatef( 0.0, area.height(), 7.5);
                }
            float vertices[] = {
                leftVertex, 0.0, -10.0,
                leftVertex + area.width(), 0.0, -10.0,
                leftVertex + area.width()*3, 0.0, -50.0,
                leftVertex - area.width()*2, 0.0, -50.0 };
            // foreground
            float alpha = 1.0;
            if( start )
                alpha = timeLine.value();
            else if( stop )
                alpha = 1.0 - timeLine.value();
            glColor4f( 0.0, 0.0, 0.0, alpha );
            glBegin( GL_POLYGON );
            glVertex3f( vertices[0], vertices[1], vertices[2] );
            glVertex3f( vertices[3], vertices[4], vertices[5] );
            // rearground
            alpha = -1.0;
            glColor4f( 0.0, 0.0, 0.0, alpha );
            glVertex3f( vertices[6], vertices[7], vertices[8] );
            glVertex3f( vertices[9], vertices[10], vertices[11] );
            glEnd();
            glPopMatrix();
            glDisable( GL_BLEND );
            }
        paintScene( frontWindow, &leftWindows, &rightWindows );
        
        glPopMatrix();
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
        QFont text_font;
        text_font.setBold( true );
        text_font.setPointSize( 20 );
        glPushAttrib( GL_CURRENT_BIT );
        glColor4f( color_frame.redF(), color_frame.greenF(), color_frame.blueF(), color_frame.alphaF());
        QRect frameRect = QRect( area.width()*0.25f + area.x(),
            area.height()*0.9f + area.y(),
            area.width()*0.5f,
            QFontMetrics( text_font ).height() * 1.2f );
        renderRoundBoxWithEdge( frameRect );
        effects->paintText( effects->currentTabBoxWindow()->caption(),
            frameRect.center(),
            frameRect.width() - 100,
            color_text,
            text_font );
        glPopAttrib();
        // icon of selected window
        QPixmap iconPixmap = effects->currentTabBoxWindow()->icon();
        if( start || stop )
            {
            int alpha = 255.0f * timeLine.value();
            if( stop )
                {
                alpha = 255.0f - alpha;
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
        glDisable( GL_BLEND );
        glPopAttrib();
        }
    }

void CoverSwitchEffect::postPaintScreen()
    {
    if( ( mActivated && ( animation || start ) ) || stop || stopRequested )
        {        
        if( timeLine.value() == 1.0 )
            {
            timeLine.setProgress(0.0);
            if( stop )
                {
                stop = false;
                effects->setActiveFullScreenEffect( 0 );
                if( startRequested )
                    {
                    startRequested = false;
                    mActivated = true;
                    effects->refTabBox();
                    selectedWindow = effects->currentTabBoxWindowList().indexOf(effects->currentTabBoxWindow());
                    if( animateStart )
                        {
                        start = true;
                        }
                    }
                }
            else if( rearrangeWindows != 0 )
                {
                if( rearrangeWindows < 0 )
                    rearrangeWindows++;
                else
                    rearrangeWindows--;
                if( start )
                    {
                    animation = true;
                    start = false;
                    }
                }
            else
                {
                animation = false;
                start = false;
                if( stopRequested )
                    {
                    stopRequested = false;
                    stop = true;
                    }
                }
            }
        effects->addRepaintFull();
        }
    effects->postPaintScreen();
    }

void CoverSwitchEffect::paintScene( EffectWindow* frontWindow, QList< EffectWindow* >* leftWindows,
    QList< EffectWindow* >* rightWindows, bool reflectedWindows )
    {
    // LAYOUT
    // one window in the front. Other windows left and right rotated
    // for odd number of windows: left: (n-1)/2; front: 1; right: (n-1)/2
    // for even number of windows: left: n/2; front: 1; right: n/2 -1
    //
    // ANIMATION
    // forward (alt+tab)
    // all left windows are moved to next position
    // top most left window is rotated and moved to front window position
    // front window is rotated and moved to next right window position
    // right windows are moved to next position
    // last right window becomes totally transparent in half the time
    // appears transparent on left side and becomes totally opaque again
    // backward (alt+shift+tab) same as forward but opposite direction
    int width = area.width();
    int height = area.height();
    int leftWindowCount = leftWindows->count();
    int rightWindowCount = rightWindows->count();


    // Problem during animation: a window which is painted after another window
    // appears in front of the other
    // so during animation the painting order has to be rearreanged
    // paint sequence no animation: left, right, front
    // paint sequence forward animation: right, front, left

    if( start || stop )
        {
        // start or stop animation
        float radian = angle * timeLine.value() * ( 2 * M_PI / 360 );
        if( stop )
            radian = ( angle - angle * timeLine.value() ) * ( 2 * M_PI / 360 );
        int x, y;
        // left windows
        for( int i=0; i< leftWindowCount; i++ )
            {
            EffectWindow* window = leftWindows->at( i );
            x = window->x();
            y = window->y();
            if( window->isMinimized() )
                {
                // use icon instead of window
                x = window->iconGeometry().x();
                y = window->iconGeometry().y();
                }
            glPushMatrix();
            int windowHeight = window->geometry().height();
            float distance = -width*0.25f + ( width * 0.25f * i )/leftWindowCount - x + area.x();
            float distanceY = height - windowHeight - y + area.y();
            if( start )
                glTranslatef( distance*timeLine.value() + x, distanceY * timeLine.value() + y, -5 * timeLine.value() - 2.5);
            else if( stop )
                glTranslatef( distance*( 1.0 - timeLine.value() ) + x, distanceY * ( 1.0 -timeLine.value() ) + y, -5 * ( 1.0 - timeLine.value() ) - 2.5);
            glRotatef( radian, 0.0, 1.0, 0.0 );
            int windowWidth = window->geometry().width() * cos( radian );
            QRect windowRect = QRect( 0, 0, windowWidth, windowHeight );
            paintWindowCover( window, windowRect, reflectedWindows );
            glPopMatrix();
            }

        // right windows
        for( int i=0; i < rightWindowCount; i++ )
            {
            EffectWindow* window = rightWindows->at( i );
            x = window->x();
            y = window->y();
            if( window->isMinimized() )
                {
                // use icon instead of window
                x = window->iconGeometry().x();
                y = window->iconGeometry().y();
                }
            glPushMatrix();
            int windowWidth = window->geometry().width() * cos( radian );
            int windowHeight = window->geometry().height();
            float distance = width*1.25f - ( width * 0.25f * i )/rightWindowCount - x - windowWidth + area.x();
            float distanceY = height - windowHeight - y + area.y();
            if( start )
                glTranslatef( distance*timeLine.value() + x + windowWidth, distanceY * timeLine.value() + y, -5 * timeLine.value() - 2.5);
            else if( stop )
                glTranslatef( distance*( 1.0 - timeLine.value() ) + x + windowWidth, distanceY * ( 1.0 - timeLine.value() ) + y, -5 * ( 1.0 - timeLine.value() ) - 2.5);
            glRotatef( -radian, 0.0, 1.0, 0.0 );
            QRect windowRect = QRect( -windowWidth, 0, windowWidth, windowHeight );
            paintWindowCover( window, windowRect, reflectedWindows );
            glPopMatrix();
            }

        // front window
        glPushMatrix();
        x = frontWindow->x();
        y = frontWindow->y();
        if( frontWindow->isMinimized() )
            {
            // use icon instead of window
            x = frontWindow->iconGeometry().x();
            y = frontWindow->iconGeometry().y();
            }
        int windowHeight = frontWindow->geometry().height();
        float distance = (width - frontWindow->geometry().width())*0.5f - x + area.x();
        float distanceY = height - windowHeight - y + area.y();
        if( start )
            glTranslatef( distance * timeLine.value() + x, distanceY * timeLine.value() + y, -5*timeLine.value() - 2.5 );
        else if( stop )
            glTranslatef( distance * ( 1.0 - timeLine.value() ) + x, distanceY * ( 1.0 - timeLine.value() ) + y, -5 * ( 1.0 - timeLine.value() ) - 2.5 );
        QRect windowRect = QRect( 0, 0, frontWindow->geometry().width(), windowHeight );
        paintWindowCover( frontWindow, windowRect, reflectedWindows );
        glPopMatrix();
        }
    else if( !animation )
        {
        paintWindows( leftWindows, true, reflectedWindows );
        paintWindows( rightWindows, false, reflectedWindows );
        paintFrontWindow( frontWindow, width, leftWindowCount, rightWindowCount, reflectedWindows );
        }
    else
        {
        if( forward )
            {
            if( timeLine.value() < 0.5 )
                {
                // paint in normal way
                paintWindows( leftWindows, true, reflectedWindows );
                paintWindows( rightWindows, false, reflectedWindows );
                paintFrontWindow( frontWindow, width, leftWindowCount, rightWindowCount, reflectedWindows );
                }
            else
                {
                paintWindows( rightWindows, false, reflectedWindows );
                EffectWindow* rightWindow;
                if( rightWindowCount > 0)
                    {
                    rightWindow = rightWindows->at( 0 );
                    paintFrontWindow( frontWindow, width, leftWindowCount, rightWindowCount, reflectedWindows );
                    }
                else
                    rightWindow = frontWindow;
                paintWindows( leftWindows, true, reflectedWindows, rightWindow );
                }
            }
        else
            {
            paintWindows( leftWindows, true, reflectedWindows );
            if( timeLine.value() < 0.5 )
                {
                paintWindows( rightWindows, false, reflectedWindows );
                paintFrontWindow( frontWindow, width, leftWindowCount, rightWindowCount, reflectedWindows );
                }
            else
                {
                paintFrontWindow( frontWindow, width, leftWindowCount, rightWindowCount, reflectedWindows );
                paintWindows( rightWindows, false, reflectedWindows, leftWindows->at( 0 ) );
                }
            }
        }
    }
  
void CoverSwitchEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( mActivated || stop || stopRequested )
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
  
void CoverSwitchEffect::tabBoxAdded( int mode )
    {
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return;
    if( !mActivated )
        {
        // only for windows mode
        if( mode == TabBoxWindowsMode && effects->currentTabBoxWindowList().count() > 0 )
            {
            if( !stop && !stopRequested )
                {
                effects->refTabBox();
                effects->setActiveFullScreenEffect( this );
                selectedWindow = effects->currentTabBoxWindowList().indexOf(effects->currentTabBoxWindow());
                mActivated = true;
                if( animateStart )
                    {
                    start = true;
                    }

                // Calculation of correct area
                area = effects->clientArea( FullScreenArea, effects->activeScreen(), effects->currentDesktop());
                QRect fullArea = effects->clientArea( FullArea, effects->activeScreen(), effects->currentDesktop());
                // twinview?
                if( area.height() != fullArea.height() || area.width() != fullArea.width() )
                    twinview = true;
                else
                    twinview = false;

                effects->addRepaintFull();
                }
            else
                {
                startRequested = true;
                }
            }
        }
    }

void CoverSwitchEffect::tabBoxClosed()
    {
    if( mActivated )
        {
        if( animateStop )
            {
            if( !animation && !start )
                {
                stop = true;
                }
            else
                {
                stopRequested = true;
                }
            }
        else
            effects->setActiveFullScreenEffect( 0 );
        mActivated = false;
        effects->unrefTabBox();
        effects->addRepaintFull();
        }
    }

void CoverSwitchEffect::tabBoxUpdated()
    {
    if( mActivated )
        {
        if( animateSwitch && effects->currentTabBoxWindowList().count() > 1)
            {
            // determine the switch direction
            int index = effects->currentTabBoxWindowList().indexOf(effects->currentTabBoxWindow());
            bool direction = false;
            int windowCount = effects->currentTabBoxWindowList().count();
            if( index > selectedWindow )
                {
                if( index == windowCount-1 && selectedWindow == 0 )
                    direction = false;
                else
                    direction = true;
                }
            else if( index == 0 && ( selectedWindow == windowCount-1 ) )
                {
                direction = true;
                }
            else if( index == selectedWindow )
                return; // nothing changed
            else
                {
                direction = false;
                }

            // for two windows direction is always forward
            if( windowCount == 2 )
                direction = true;
            selectedWindow = index;
            if( !animation && !start )
                {
                forward = direction;
                animation = true;
                }
            else
                {
                if( direction )
                    rearrangeWindows--;
                else
                    rearrangeWindows++;
                if( rearrangeWindows >= windowCount )
                    rearrangeWindows = rearrangeWindows % windowCount;
                else if( (-1*rearrangeWindows) >= windowCount )
                    rearrangeWindows = -1*((-1*rearrangeWindows) % windowCount);
                }
            }
        effects->addRepaintFull();
        }
    }

void CoverSwitchEffect::paintWindowCover( EffectWindow* w, QRect windowRect, bool reflectedWindow, float opacity )
    {
    WindowPaintData data( w );

    data.opacity = opacity;

    QRect thumbnail = infiniteRegion();
    setPositionTransformations( data,
        thumbnail, w,
        windowRect,
        Qt::IgnoreAspectRatio );
    thumbnail = infiniteRegion();

    if( reflectedWindow )
        {
        glPushMatrix();
        glScalef( 1.0, -1.0, 1.0 );
        glTranslatef( 0.0, - area.height() - windowRect.y() - windowRect.height(), 0.0 );
        effects->paintWindow( w,
            PAINT_WINDOW_TRANSFORMED,
            thumbnail, data );
        glPopMatrix();
        }
    else
        {
        effects->paintWindow( w,
            PAINT_WINDOW_TRANSFORMED,
            thumbnail, data );
        }
    }

void CoverSwitchEffect::paintFrontWindow( EffectWindow* frontWindow, int width, int leftWindows, int rightWindows, bool reflectedWindow )
    {
    glPushMatrix();
    glTranslatef((width - frontWindow->geometry().width())*0.5f, 0.0, -7.5);
    int windowWidth = frontWindow->geometry().width();
    int windowHeight = frontWindow->geometry().height();;
    float distance = 0.0;
    int height = area.height();
    int x = 0;
    bool specialHandlingForward = false;
    if( leftWindows == 0 )
        leftWindows = 1;
    if( rightWindows == 0 )
        {
        rightWindows = 1;
        specialHandlingForward = true;
        }
    if( animation )
        {
        float radian = 0.0;
        radian = angle * timeLine.value() * ( 2 * M_PI / 360 );
        windowWidth = frontWindow->geometry().width() * cos( radian );
        if( forward )
            {
            x = - windowWidth;
            glTranslatef( frontWindow->geometry().width(), 0.0, 0.0 );
            // move to right
            // we are at: (width + frontWindow->geometry().width())*0.5f
            // we want to: width*1.25 - ( width * 0.25 *  (rightWindowCount -1) )/rightWindowCount
            distance = width*1.25 - ( width * 0.25 *  ( rightWindows - 1 ) )/rightWindows -
                (width + frontWindow->geometry().width())*0.5f;
            glTranslatef( distance * timeLine.value(), 0.0, 0.0 );
            glRotatef(-radian, 0.0, 1.0, 0.0);
            }
        else
            {
            // move to left
            // we are at: (width - frontWindow->geometry().width())*0.5f
            // we want to: -width*0.25 + ( width * 0.25 * leftWindowCount - 1 )/leftWindowCount
            distance = ( width - frontWindow->geometry().width() ) * 0.5f +
                width*0.25 - ( width * 0.25 * ( leftWindows - 1 ) )/leftWindows;
            glTranslatef( - distance * timeLine.value(), 0.0, 0.0 );
            glRotatef(radian, 0.0, 1.0, 0.0);
            }
        }
    QRect windowRect = QRect( x, height - windowHeight, windowWidth, windowHeight );
    if( specialHandlingForward )
        paintWindowCover( frontWindow, windowRect, reflectedWindow, 1.0 - timeLine.value() * 2 );
    else
        paintWindowCover( frontWindow, windowRect, reflectedWindow );
    glPopMatrix();
    }

void CoverSwitchEffect::paintWindows( QList< EffectWindow* >* windows, bool left, bool reflectedWindows, EffectWindow* additionalWindow )
    {
    int width = area.width();
    int height = area.height();
    float radian = angle * ( 2 * M_PI / 360 );
    int windowCount = windows->count();
    int windowWidth = 0;
    int windowHeight = 0;
    QRect windowRect;
    EffectWindow* window;
    
    int rotateFactor = 1;
    float widthFactor = -0.25;
    float widthFactorSingle = 0.25;
    if( !left )
        {
        rotateFactor = -1;
        widthFactor = 1.25;
        widthFactorSingle = - 0.25;
        }
    
    glPushMatrix();
    glTranslatef( width*widthFactor, 0.0, -7.5 );
    // handling for additional window from other side
    // has to appear on this side after half of the time
    if( animation && timeLine.value() >= 0.5 && additionalWindow != NULL )
        {
        // window has to appear on left side
        glPushMatrix();
        glRotatef( radian * rotateFactor, 0.0, 1.0, 0.0 );
        windowWidth = additionalWindow->geometry().width() * cos( radian );
        windowHeight = additionalWindow->geometry().height();
        int x = 0;
        if( !left )
            {
            x = -windowWidth;
            }
        windowRect = QRect( x, height - windowHeight, windowWidth, windowHeight );
        paintWindowCover( additionalWindow, windowRect, reflectedWindows, ( timeLine.value() - 0.5 ) * 2 );
        glPopMatrix();
        }
    // normal behaviour
    for( int i=0; i < windowCount; i++ )
        {
        window = windows->at( i );
        glPushMatrix();
        glTranslatef( ( width * widthFactorSingle * i )/windowCount, 0.0, 0.0 );
        if( animation )
            {
            if( forward )
                {
                if( ( i == windowCount - 1 ) && left )
                    {
                    // right most window on left side -> move to front
                    // we are at: -width*0.25 + ( width * 0.25 * i )/leftWindowCount
                    // we want to: (width - leftWindow->geometry().width())*0.5f
                    float distance = (width - window->geometry().width())*0.5f + width*(-widthFactor) - ( width * widthFactorSingle * i )/windowCount;
                    glTranslatef( distance * timeLine.value() , 0.0, 0.0 );
                    radian = ( angle - angle * timeLine.value() )  * ( 2 * M_PI / 360 );
                    }
                // right most window does not have to be moved
                else if( !left && ( i == 0 ) ); // do nothing
                else
                    {
                    // all other windows - move to next position
                    glTranslatef( ( width * 0.25 * timeLine.value() )/windowCount, 0.0, 0.0 );
                    }
                }
            else
                {
                if( ( i == windowCount - 1 ) && !left )
                    {
                    // left most window on right side -> move to front
                    // we are at: width*1.25 - ( width * 0.25 *  i )/rightWindowCount
                    // we want to: (width + rightWindow->geometry().width())*0.5f
                    float distance = width*1.25 - ( width * 0.25 *  i )/windowCount - 
                        (width + window->geometry().width())*0.5f;
                    glTranslatef( - distance * timeLine.value(), 0.0, 0.0 );
                    radian = ( angle - angle * timeLine.value() ) * ( 2 * M_PI / 360 );
                    }
                // left most window does not have to be moved
                else if( i==0 && left); // do nothing
                else
                    {
                    // all other windows - move to next position
                    glTranslatef( - ( width * 0.25 * timeLine.value() )/windowCount, 0.0, 0.0 );
                    }
                }
            }
        glRotatef( rotateFactor * radian, 0.0, 1.0, 0.0 );
        windowWidth = window->geometry().width() * cos( radian );
        windowHeight = window->geometry().height();
        int x = 0;
        if( !left )
            {
            x = -windowWidth;
            }
        windowRect = QRect( x, height - windowHeight, windowWidth, windowHeight );
        // make window most to edge transparent if animation
        if( animation && i == 0 && ( ( !forward && left ) || ( forward && !left ) ) )
            {
            // only for the first half of the animation
            if( timeLine.value() < 0.5 )
                paintWindowCover( window, windowRect, reflectedWindows, 1.0 - timeLine.value() * 2 );
            }
        else
            paintWindowCover( window, windowRect, reflectedWindows );
        glPopMatrix();
        }
    glPopMatrix();
    }

} // namespace
