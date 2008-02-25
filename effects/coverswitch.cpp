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
#include "coverswitch.h"

#include <kwinconfig.h>
#include <QApplication>
#include <QDesktopWidget>
#include <QFont>
#include <QBitmap>
#include <kaction.h>
#include <kactioncollection.h>
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
    , slowMotionMode( false )
    , stopRequested( false )
    , startRequested( false )
    {
    KConfigGroup conf = effects->effectConfig( "CoverSwitch" );
    animationDuration = conf.readEntry( "Duration", 300 );
    animateSwitch     = conf.readEntry( "AnimateSwitch", true );
    animateStart      = conf.readEntry( "AnimateStart", true );
    animateStop       = conf.readEntry( "AnimateStop", true );
    reflection        = conf.readEntry( "Reflection", true );
    slowMotionFactor   = conf.readEntry( "SlowMotionFactor", 4 );

    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = static_cast< KAction* >( actionCollection->addAction( "SlowMotion" ));
    a->setText( i18n( "Slow Motion" ));
    a->setGlobalShortcut( KShortcut( Qt::META + Qt::Key_S ));
    connect( a, SIGNAL( triggered( bool )), this, SLOT( slowMotion()));
    }

CoverSwitchEffect::~CoverSwitchEffect()
    {
    }

void CoverSwitchEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( mActivated || stop || stopRequested )
        {
        data.mask |= Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
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
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT );
        glEnable( GL_DEPTH_TEST );
        glLoadIdentity();
        glFrustum(-QApplication::desktop()->geometry().width()*0.5f,
            QApplication::desktop()->geometry().width()*0.5f,
            QApplication::desktop()->geometry().height()*0.5f,
            -QApplication::desktop()->geometry().height()*0.5f, 10, 50);
        
        glMatrixMode( GL_MODELVIEW );
        glLoadIdentity();
        glPushMatrix();
        glTranslatef(-QApplication::desktop()->geometry().width()*0.5f,
            -QApplication::desktop()->geometry().height()*0.5f,
            -7.5);
    
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
    
        // time since last animation
        int elapsed = 0;
        float timeFactor = 0.0;
        if( animation || start || stop )
            {
            elapsed = animationTime.elapsed();
            timeFactor = (float)((float)elapsed/(float)animationDuration);
            if( timeFactor >= 1.0 )
                timeFactor = 1.0;
            }

        if( reflection )
            {
            // no reflections during start and stop animation
            if( !start && !stop )
                paintScene( frontWindow, &leftWindows, &rightWindows, timeFactor, true );
            glEnable( GL_BLEND );
            glBlendFunc( GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA );
            glPolygonMode( GL_FRONT, GL_FILL );
            glBegin( GL_POLYGON );
            float alpha = 0.9;
            if( start )
                alpha = 0.9 * timeFactor;
            else if( stop )
                alpha = 0.9 - 0.9 * timeFactor;
            glColor4f( 0.3, 0.3, 0.3, alpha );
            glVertex3f( 0.0, QApplication::desktop()->geometry().height(), 0.0 );
            glVertex3f( QApplication::desktop()->geometry().width(), QApplication::desktop()->geometry().height(), 0.0 );
            alpha = 1.0;
            if( start )
                alpha = timeFactor;
            else if( stop )
                alpha = 1.0 - timeFactor;
            glColor4f( 0.19, 0.19, 0.19, alpha );
            glVertex3f( QApplication::desktop()->geometry().width() * 5, QApplication::desktop()->geometry().height(), -60 );
            glVertex3f( -QApplication::desktop()->geometry().width() * 4, QApplication::desktop()->geometry().height(), -60 );
            glEnd();
            glDisable( GL_BLEND );
            }
        paintScene( frontWindow, &leftWindows, &rightWindows, timeFactor );
        
        glPopMatrix();
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
        if( start )
            {
            color_frame.setAlphaF( 0.9 * timeFactor );
            color_text.setAlphaF( timeFactor );
            }
        else if( stop )
            {
            color_frame.setAlphaF( 0.9 - 0.9 * timeFactor );
            color_text.setAlphaF( 1.0 - timeFactor );
            }
        QFont text_font;
        text_font.setBold( true );
        text_font.setPointSize( 20 );
        glPushAttrib( GL_CURRENT_BIT );
        glColor4f( color_frame.redF(), color_frame.greenF(), color_frame.blueF(), color_frame.alphaF());
        QRect frameRect = QRect( QApplication::desktop()->geometry().width()*0.25f,
            QApplication::desktop()->geometry().height()*0.9f,
            QApplication::desktop()->geometry().width()*0.5f,
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
            int alpha = 255.0f * timeFactor;
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
        if( animationTime.elapsed() >= animationDuration )
            {
            if( stop )
                {
                stop = false;
                if( startRequested )
                    {
                    startRequested = false;
                    mActivated = true;
                    effects->refTabBox();
                    selectedWindow = effects->currentTabBoxWindowList().indexOf(effects->currentTabBoxWindow());
                    if( animateStart )
                        {
                        start = true;
                        animationTime.restart();
                        }
                    }
                }
            else if( rearrangeWindows != 0 )
                {
                if( rearrangeWindows < 0 )
                    rearrangeWindows++;
                else
                    rearrangeWindows--;
                animationTime.restart();
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
                    animationTime.restart();
                    }
                }
            }
        effects->addRepaintFull();
        }
    effects->postPaintScreen();
    }

void CoverSwitchEffect::paintScene( EffectWindow* frontWindow, QList< EffectWindow* >* leftWindows, QList< EffectWindow* >* rightWindows, 
    float timeFactor, bool reflectedWindows )
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
    int width = QApplication::desktop()->geometry().width();
    int height = QApplication::desktop()->geometry().height();
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
        float radian = angle * timeFactor * ( 2 * M_PI / 360 );
        if( stop )
            radian = ( angle - angle * timeFactor ) * ( 2 * M_PI / 360 );
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
            float distance = -width*0.25f + ( width * 0.25f * i )/leftWindowCount - x;
            float distanceY = height - windowHeight - y;
            if( start )
                glTranslatef( distance*timeFactor + x, distanceY * timeFactor + y, -5 * timeFactor - 2.5);
            else if( stop )
                glTranslatef( distance*( 1.0 - timeFactor ) + x, distanceY * ( 1.0 -timeFactor ) + y, -5 * ( 1.0 - timeFactor ) - 2.5);
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
            float distance = width*1.25f - ( width * 0.25f * i )/rightWindowCount - x - windowWidth;
            float distanceY = height - windowHeight - y;
            if( start )
                glTranslatef( distance*timeFactor + x + windowWidth, distanceY * timeFactor + y, -5 * timeFactor - 2.5);
            else if( stop )
                glTranslatef( distance*( 1.0 - timeFactor ) + x + windowWidth, distanceY * ( 1.0 - timeFactor ) + y, -5 * ( 1.0 - timeFactor ) - 2.5);
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
        float distance = (width - frontWindow->geometry().width())*0.5f - x;
        float distanceY = height - windowHeight - y;
        if( start )
            glTranslatef( distance * timeFactor + x, distanceY * timeFactor + y, -5*timeFactor - 2.5 );
        else if( stop )
            glTranslatef( distance * ( 1.0 - timeFactor ) + x, distanceY * ( 1.0 - timeFactor ) + y, -5 * ( 1.0 - timeFactor ) - 2.5 );
        QRect windowRect = QRect( 0, 0, frontWindow->geometry().width(), windowHeight );
        paintWindowCover( frontWindow, windowRect, reflectedWindows );
        glPopMatrix();
        }
    else if( !animation )
        {
        paintWindows( leftWindows, timeFactor, true, reflectedWindows );
        paintWindows( rightWindows, timeFactor, false, reflectedWindows );
        paintFrontWindow( frontWindow, timeFactor, width, leftWindowCount, rightWindowCount, reflectedWindows );
        }
    else
        {
        if( forward )
            {
            if( timeFactor < 0.5 )
                {
                // paint in normal way
                paintWindows( leftWindows, timeFactor, true, reflectedWindows );
                paintWindows( rightWindows, timeFactor, false, reflectedWindows );
                paintFrontWindow( frontWindow, timeFactor, width, leftWindowCount, rightWindowCount, reflectedWindows );
                }
            else
                {
                paintWindows( rightWindows, timeFactor, false, reflectedWindows );
                EffectWindow* rightWindow;
                if( rightWindowCount > 0)
                    {
                    rightWindow = rightWindows->at( 0 );
                    paintFrontWindow( frontWindow, timeFactor, width, leftWindowCount, rightWindowCount, reflectedWindows );
                    }
                else
                    rightWindow = frontWindow;
                paintWindows( leftWindows, timeFactor, true, reflectedWindows, rightWindow );
                }
            }
        else
            {
            paintWindows( leftWindows, timeFactor, true, reflectedWindows );
            if( timeFactor < 0.5 )
                {
                paintWindows( rightWindows, timeFactor, false, reflectedWindows );
                paintFrontWindow( frontWindow, timeFactor, width, leftWindowCount, rightWindowCount, reflectedWindows );
                }
            else
                {
                paintFrontWindow( frontWindow, timeFactor, width, leftWindowCount, rightWindowCount, reflectedWindows );
                paintWindows( rightWindows, timeFactor, false, reflectedWindows, leftWindows->at( 0 ) );
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
                int elapsed = animationTime.elapsed();
                float timeFactor = (float)((float)elapsed/(float)animationDuration);
                if( timeFactor >= 1.0 )
                    timeFactor = 1.0;
                data.opacity = 1.0 - timeFactor;
                if( stop )
                    data.opacity = timeFactor;
                }
            else
                return;
            }
        }
    effects->paintWindow( w, mask, region, data );
    }
  
void CoverSwitchEffect::tabBoxAdded( int mode )
    {
    if( !mActivated )
        {
        // only for windows mode
        if( mode == TabBoxWindowsMode && effects->currentTabBoxWindowList().count() > 0 )
            {
            if( !stop && !stopRequested )
                {
                effects->refTabBox();
                selectedWindow = effects->currentTabBoxWindowList().indexOf(effects->currentTabBoxWindow());
                mActivated = true;
                if( animateStart )
                    {
                    start = true;
                    animationTime.restart();
                    }
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
                animationTime.restart();
                }
            else
                {
                stopRequested = true;
                }
            }
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
                animationTime.restart();
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

    QRect thumbnail;
    setPositionTransformations( data,
        thumbnail, w,
        windowRect,
        Qt::IgnoreAspectRatio );

    if( reflectedWindow )
        {
        glPushMatrix();
        glScalef( 1.0, -1.0, 1.0 );
        glTranslatef( 0.0, - QApplication::desktop()->geometry().height() - windowRect.y() - windowRect.height(), 0.0 );
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

void CoverSwitchEffect::paintFrontWindow( EffectWindow* frontWindow, float timeFactor, int width, int leftWindows, int rightWindows, bool reflectedWindow )
    {
    glPushMatrix();
    glTranslatef((width - frontWindow->geometry().width())*0.5f, 0.0, -7.5);
    int windowWidth = frontWindow->geometry().width();
    int windowHeight = frontWindow->geometry().height();;
    float distance = 0.0;
    int height = QApplication::desktop()->geometry().height();
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
        if( specialHandlingForward )
            timeFactor *= 2;
        radian = angle * timeFactor * ( 2 * M_PI / 360 );
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
            glTranslatef( distance * timeFactor, 0.0, 0.0 );
            glRotatef(-radian, 0.0, 1.0, 0.0);
            }
        else
            {
            // move to left
            // we are at: (width - frontWindow->geometry().width())*0.5f
            // we want to: -width*0.25 + ( width * 0.25 * leftWindowCount - 1 )/leftWindowCount
            distance = ( width - frontWindow->geometry().width() ) * 0.5f +
                width*0.25 - ( width * 0.25 * ( leftWindows - 1 ) )/leftWindows;
            glTranslatef( - distance * timeFactor, 0.0, 0.0 );
            glRotatef(radian, 0.0, 1.0, 0.0);
            }
        }
    QRect windowRect = QRect( x, height - windowHeight, windowWidth, windowHeight );
    if( specialHandlingForward )
        paintWindowCover( frontWindow, windowRect, reflectedWindow, 1.0 - timeFactor );
    else
        paintWindowCover( frontWindow, windowRect, reflectedWindow );
    glPopMatrix();
    }

void CoverSwitchEffect::paintWindows( QList< EffectWindow* >* windows, float timeFactor, bool left, bool reflectedWindows, EffectWindow* additionalWindow )
    {
    int width = QApplication::desktop()->geometry().width();
    int height = QApplication::desktop()->geometry().height();
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
    if( animation && timeFactor >= 0.5 && additionalWindow != NULL )
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
        paintWindowCover( additionalWindow, windowRect, reflectedWindows, ( timeFactor - 0.5 ) * 2 );
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
                    glTranslatef( distance * timeFactor , 0.0, 0.0 );
                    radian = ( angle - angle * timeFactor )  * ( 2 * M_PI / 360 );
                    }
                // right most window does not have to be moved
                else if( !left && ( i == 0 ) ); // do nothing
                else
                    {
                    // all other windows - move to next position
                    glTranslatef( ( width * 0.25 * timeFactor )/windowCount, 0.0, 0.0 );
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
                    glTranslatef( - distance * timeFactor, 0.0, 0.0 );
                    radian = ( angle - angle * timeFactor ) * ( 2 * M_PI / 360 );
                    }
                // left most window does not have to be moved
                else if( i==0 && left); // do nothing
                else
                    {
                    // all other windows - move to next position
                    glTranslatef( - ( width * 0.25 * timeFactor )/windowCount, 0.0, 0.0 );
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
            if( timeFactor < 0.5 )
                paintWindowCover( window, windowRect, reflectedWindows, 1.0 - timeFactor * 2 );
            }
        else
            paintWindowCover( window, windowRect, reflectedWindows );
        glPopMatrix();
        }
    glPopMatrix();
    }

void CoverSwitchEffect::slowMotion()
    {
    slowMotionMode = !slowMotionMode;
    if( slowMotionMode )
        {
        animationDuration *= slowMotionFactor;
        }
    else
        {
        animationDuration /= slowMotionFactor;
        }
    }

} // namespace
