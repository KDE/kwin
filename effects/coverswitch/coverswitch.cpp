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
KWIN_EFFECT_SUPPORTED( coverswitch, CoverSwitchEffect::supported() )

CoverSwitchEffect::CoverSwitchEffect()
    : mActivated( 0 )
    , angle( 60.0 )
    , animation( false )
    , start( false )
    , stop( false )
    , stopRequested( false )
    , startRequested( false )
    , zPosition( 900.0 )
    , scaleFactor( 0.0 )
    , direction( Left )
    , selected_window( 0 )
    , captionFrame( EffectFrame::Styled )
    , thumbnailFrame( EffectFrame::Styled )
    {
    reconfigure( ReconfigureAll );

    // Caption frame
    captionFont.setBold( true );
    captionFont.setPointSize( captionFont.pointSize() * 2 );
    captionFrame.setFont( captionFont );
    }

CoverSwitchEffect::~CoverSwitchEffect()
    {
    qDeleteAll( windows );
    }

bool CoverSwitchEffect::supported()
    {
    return effects->compositingType() == OpenGLCompositing;
    }

void CoverSwitchEffect::reconfigure( ReconfigureFlags )
    {
    KConfigGroup conf = effects->effectConfig( "CoverSwitch" );
    animationDuration = animationTime( conf, "Duration", 200 );
    animateSwitch     = conf.readEntry( "AnimateSwitch", true );
    animateStart      = conf.readEntry( "AnimateStart", true );
    animateStop       = conf.readEntry( "AnimateStop", true );
    reflection        = conf.readEntry( "Reflection", true );
    windowTitle       = conf.readEntry( "WindowTitle", true );
    zPosition         = conf.readEntry( "ZPosition", 900.0 );
    thumbnails        = conf.readEntry( "Thumbnails", true );
    dynamicThumbnails = conf.readEntry( "DynamicThumbnails", true );
    thumbnailWindows  = conf.readEntry( "ThumbnailWindows", 8 );
    timeLine.setCurveShape( TimeLine::EaseInOutCurve );
    timeLine.setDuration( animationDuration );

    // thumbnail bar
    color_frame = KColorScheme( QPalette::Active, KColorScheme::Window ).background().color();
    color_frame.setAlphaF( 0.9 );
    color_highlight = KColorScheme( QPalette::Active, KColorScheme::Selection ).background().color();
    color_highlight.setAlphaF( 0.9 );
    highlight_margin = 10;
    }

void CoverSwitchEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( mActivated || stop || stopRequested )
        {
        data.mask |= Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        if( animation || start || stop )
            {
            timeLine.addTime( (double)time );
            if( thumbnails && (!dynamicThumbnails || 
                (dynamicThumbnails && currentWindowList.size() >= thumbnailWindows)) )
                calculateItemSizes();
            }
        if( effects->currentTabBoxWindow() == NULL )
            abort();
        }
    effects->prePaintScreen(data, time);
    }

void CoverSwitchEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );

    if( mActivated || stop || stopRequested )
        {
        if( effects->numScreens() > 1 )
            {
            // unfortunatelly we have to change the projection matrix in dual screen mode
            QRect fullRect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
            glMatrixMode( GL_PROJECTION );
            glPushMatrix();
            glLoadIdentity();
            float fovy = 60.0f;
            float aspect = 1.0f;
            float zNear = 0.1f;
            float zFar = 100.0f;
            float ymax = zNear * tan( fovy  * M_PI / 360.0f );
            float ymin = -ymax;
            float xmin =  ymin * aspect;
            float xmax = ymax * aspect;
            float xTranslate = 0.0;
            float yTranslate = 0.0;
            float xminFactor = 1.0;
            float xmaxFactor = 1.0;
            float yminFactor = 1.0;
            float ymaxFactor = 1.0;
            if( area.x() == 0 && area.width() != fullRect.width() )
                {
                // horizontal layout: left screen
                xminFactor = (float)area.width()/(float)fullRect.width();
                xmaxFactor = ((float)fullRect.width()-(float)area.width()*0.5f)/((float)fullRect.width()*0.5f);
                xTranslate = (float)fullRect.width()*0.5f-(float)area.width()*0.5f;
                }
            if( area.x() != 0 && area.width() != fullRect.width() )
                {
                // horizontal layout: right screen
                xminFactor = ((float)fullRect.width()-(float)area.width()*0.5f)/((float)fullRect.width()*0.5f);
                xmaxFactor = (float)area.width()/(float)fullRect.width();
                xTranslate = (float)fullRect.width()*0.5f-(float)area.width()*0.5f;
                }
            if( area.y() == 0 && area.height() != fullRect.height() )
                {
                // vertical layout: top screen
                yminFactor = ((float)fullRect.height()-(float)area.height()*0.5f)/((float)fullRect.height()*0.5f);
                ymaxFactor = (float)area.height()/(float)fullRect.height();
                yTranslate = (float)fullRect.height()*0.5f-(float)area.height()*0.5f;
                }
            if( area.y() != 0 && area.height() != fullRect.height() )
                {
                // vertical layout: bottom screen
                yminFactor = (float)area.height()/(float)fullRect.height();
                ymaxFactor = ((float)fullRect.height()-(float)area.height()*0.5f)/((float)fullRect.height()*0.5f);
                yTranslate = (float)fullRect.height()*0.5f-(float)area.height()*0.5f;
                }
            glFrustum( xmin*xminFactor, xmax*xmaxFactor, ymin*yminFactor, ymax*ymaxFactor, zNear, zFar );
            glMatrixMode( GL_MODELVIEW );
            glPushMatrix();
            glTranslatef( xTranslate, yTranslate, 0.0 );
            }
    
        QList< EffectWindow* > tempList = currentWindowList;
        int index = tempList.indexOf( selected_window );
        if( animation || start || stop )
            {
            if( !start && !stop )
                {
                if( direction == Right )
                    index++;
                else
                    index--;
                if( index < 0 )
                    index = tempList.count() + index;
                if( index >= tempList.count() )
                    index = index % tempList.count();
                }
            foreach( Direction direction, scheduled_directions )
                {
                if( direction == Right )
                    index++;
                else
                    index--;
                if( index < 0 )
                    index = tempList.count() + index;
                if( index >= tempList.count() )
                    index = index % tempList.count();
                }
            }
        int leftIndex = index -1;
        if( leftIndex < 0 )
            leftIndex = tempList.count() -1;
        int rightIndex = index +1;
        if( rightIndex == tempList.count() )
            rightIndex = 0;

        EffectWindow* frontWindow = tempList[ index ];
        leftWindows.clear();
        rightWindows.clear();

        bool evenWindows = ( tempList.count() % 2 == 0 ) ? true : false;
        int leftWindowCount = 0;
        if( evenWindows )
            leftWindowCount = tempList.count()/2 - 1;
        else
            leftWindowCount = ( tempList.count() - 1 )/2;
        for( int i=0; i < leftWindowCount; i++ )
            {
            int tempIndex = ( leftIndex - i );
            if( tempIndex < 0 )
                tempIndex = tempList.count() + tempIndex;
            leftWindows.prepend( tempList[ tempIndex ] );
            }
        int rightWindowCount = 0;
        if( evenWindows )
            rightWindowCount = tempList.count()/2;
        else
            rightWindowCount = ( tempList.count() - 1 )/2;
        for( int i=0; i < rightWindowCount; i++ )
            {
            int tempIndex = ( rightIndex + i ) % tempList.count();
            rightWindows.prepend( tempList[ tempIndex ] );
            }

        if( reflection )
            {
            // restrict painting the reflections to the current screen
            QRegion clip = QRegion( area );
            PaintClipper::push( clip );
            // no reflections during start and stop animation
            if( !start && !stop )
                paintScene( frontWindow, leftWindows, rightWindows, true );
            PaintClipper::pop( clip );
            glEnable( GL_BLEND );
            glBlendFunc( GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA );
            glPolygonMode( GL_FRONT, GL_FILL );
            glPushMatrix();
            QRect fullRect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
            // we can use a huge scale factor (needed to calculate the rearground vertices)
            // as we restrict with a PaintClipper painting on the current screen
            float reflectionScaleFactor = 100000 * tan( 60.0 * M_PI / 360.0f )/area.width();
            if( effects->numScreens() > 1 && area.x() != fullRect.x() )
                {
                // have to change the reflection area in horizontal layout and right screen
                glTranslatef( -area.x(), 0.0, 0.0 );
                }
            glTranslatef( area.x() + area.width()*0.5f, 0.0, 0.0 );
            float vertices[] = {
                -area.width()*0.5f, area.height(), 0.0,
                area.width()*0.5f, area.height(), 0.0,
                (float)area.width()*reflectionScaleFactor, area.height(), -5000,
                -(float)area.width()*reflectionScaleFactor, area.height(), -5000 };
            // foreground
            float alpha = 1.0;
            if( start )
                alpha = timeLine.value();
            else if( stop )
                alpha = 1.0 - timeLine.value();
            glColor4f( 0.0, 0.0, 0.0, alpha );

            int y = 0;
            // have to adjust the y values to fit OpenGL
            // in OpenGL y==0 is at bottom, in Qt at top
            if( effects->numScreens() > 1 )
                {
                QRect fullArea = effects->clientArea( FullArea, 0, 1 );
                if( fullArea.height() != area.height() )
                    {
                    if( area.y() == 0 )
                        y = fullArea.height() - area.height();
                    else
                        y = fullArea.height() - area.y() - area.height();
                    }
                }
            // use scissor to restrict painting of the reflection plane to current screen
            glScissor( area.x(), y, area.width(), area.height() );
            glEnable( GL_SCISSOR_TEST );
            glBegin( GL_POLYGON );
            glVertex3f( vertices[0], vertices[1], vertices[2] );
            glVertex3f( vertices[3], vertices[4], vertices[5] );
            // rearground
            alpha = -1.0;
            glColor4f( 0.0, 0.0, 0.0, alpha );
            glVertex3f( vertices[6], vertices[7], vertices[8] );
            glVertex3f( vertices[9], vertices[10], vertices[11] );
            glEnd();
            glDisable( GL_SCISSOR_TEST );

            glPopMatrix();
            glDisable( GL_BLEND );
            }
        paintScene( frontWindow, leftWindows, rightWindows );

        if( effects->numScreens() > 1 )
            {
            glPopMatrix();
            // revert change of projection matrix
            glMatrixMode( GL_PROJECTION );
            glPopMatrix();
            glMatrixMode( GL_MODELVIEW );
            }

        // Render the caption frame
        if (windowTitle)
        {
            double opacity = 1.0;
            if( start )
                opacity = timeLine.value();
            else if( stop )
                opacity = 1.0 - timeLine.value();
            captionFrame.render( region, opacity );
        }

        if( ( thumbnails && (!dynamicThumbnails || 
            (dynamicThumbnails && currentWindowList.size() >= thumbnailWindows)) )
            && !( start || stop ) )
            {
            thumbnailFrame.render( region );
            // HACK: PaintClipper is used because window split is somehow wrong if the height is greater than width
            PaintClipper::push( frame_area );
            paintHighlight( highlight_area );
            foreach( EffectWindow* w, windows.keys())
                {
                paintWindowThumbnail( w );
                paintWindowIcon( w );
                }
            PaintClipper::pop( frame_area );
            }
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
                foreach( EffectWindow* window, referrencedWindows )
                    {
                    window->unrefWindow();
                    }
                referrencedWindows.clear();
                currentWindowList.clear();
                if( startRequested )
                    {
                    startRequested = false;
                    mActivated = true;
                    effects->refTabBox();
                    currentWindowList = effects->currentTabBoxWindowList();
                    if( animateStart )
                        {
                        start = true;
                        }
                    }
                }
            else if( !scheduled_directions.isEmpty() )
                {
                direction = scheduled_directions.dequeue();
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

void CoverSwitchEffect::paintScene( EffectWindow* frontWindow, const EffectWindowList& leftWindows,
    const EffectWindowList& rightWindows, bool reflectedWindows )
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
    int leftWindowCount = leftWindows.count();
    int rightWindowCount = rightWindows.count();
    RotationData rot;
    rot.axis = RotationData::YAxis;


    // Problem during animation: a window which is painted after another window
    // appears in front of the other
    // so during animation the painting order has to be rearreanged
    // paint sequence no animation: left, right, front
    // paint sequence forward animation: right, front, left

    if( !animation )
        {
        paintWindows( leftWindows, true, reflectedWindows );
        paintWindows( rightWindows, false, reflectedWindows );
        paintFrontWindow( frontWindow, width, leftWindowCount, rightWindowCount, reflectedWindows );
        }
    else
        {
        if( direction == Right )
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
                paintFrontWindow( frontWindow, width, leftWindowCount, rightWindowCount, reflectedWindows );
                paintWindows( leftWindows, true, reflectedWindows, rightWindows.at( 0 ) );
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
                EffectWindow* leftWindow;
                if( leftWindowCount > 0)
                    {
                    leftWindow = leftWindows.at( 0 );
                    paintFrontWindow( frontWindow, width, leftWindowCount, rightWindowCount, reflectedWindows );
                    }
                else
                    leftWindow = frontWindow;
                paintWindows( rightWindows, false, reflectedWindows, leftWindow );
                }
            }
        }
    }

void CoverSwitchEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( mActivated || stop || stopRequested )
        {
        if( !( mask & PAINT_WINDOW_TRANSFORMED ) && !w->isDesktop() )
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
    if ( ( start || stop ) && (!w->isOnCurrentDesktop() || w->isMinimized() ) )
        {
        if (stop) // Fade out windows not on the current desktop
            data.opacity = (1.0 - timeLine.value());
        else // Fade in Windows from other desktops when animation is started
            data.opacity = timeLine.value();
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
            input = effects->createFullScreenInputWindow( this, Qt::ArrowCursor );
            activeScreen = effects->activeScreen();
            if( !stop && !stopRequested )
                {
                effects->refTabBox();
                effects->setActiveFullScreenEffect( this );
                scheduled_directions.clear();
                selected_window = effects->currentTabBoxWindow();
                currentWindowList = effects->currentTabBoxWindowList();
                direction = Left;
                mActivated = true;
                if( animateStart )
                    {
                    start = true;
                    }

                // Calculation of correct area
                area = effects->clientArea( FullScreenArea, activeScreen, effects->currentDesktop());
                scaleFactor = (zPosition+1100) * 2.0 * tan( 60.0 * M_PI / 360.0f )/displayWidth();
                if( displayWidth()-area.width() != 0 )
                    {
                    // one of the screens is smaller than the other (horizontal)
                    if( area.width() < displayWidth() - area.width() )
                        scaleFactor *= (float)area.width()/(float)(displayWidth()-area.width());
                    else if( area.width() != displayWidth() - area.width() )
                        {
                        // vertical layout with different width
                        // but we don't want to catch screens with same width and different height
                        if( displayHeight() != area.height() )
                            scaleFactor *= (float)area.width()/(float)(displayWidth());
                        }
                    }

                // Setup caption frame geometry
                if (windowTitle)
                {
                    QRect frameRect = QRect( area.width() * 0.25f + area.x(),
                        area.height() * 0.9f + area.y(),
                        area.width() * 0.5f,
                        QFontMetrics( captionFont ).height() );
                    captionFrame.setGeometry( frameRect );
                    captionFrame.setIconSize( QSize( frameRect.height(), frameRect.height() ));
                    // And initial contents
                    captionFrame.setText( selected_window->caption() );
                    captionFrame.setIcon( selected_window->icon() );
                }

                effects->addRepaintFull();
                }
            else
                {
                startRequested = true;
                }
            if( thumbnails && (!dynamicThumbnails || 
                (dynamicThumbnails && currentWindowList.size() >= thumbnailWindows)) )
                {
                highlight_is_set = false;
                calculateFrameSize();
                calculateItemSizes();
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
            else if( start && scheduled_directions.isEmpty() )
                {
                start = false;
                stop = true;
                timeLine.setProgress( 1.0 - timeLine.value() );
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
        effects->destroyInputWindow( input );
        effects->addRepaintFull();
        if( thumbnails && (!dynamicThumbnails || 
                (dynamicThumbnails && currentWindowList.size() >= thumbnailWindows)) )
            {
            qDeleteAll( windows );
            windows.clear();
            }
        }
    }

void CoverSwitchEffect::tabBoxUpdated()
    {
    if( mActivated )
        {
        if( animateSwitch && currentWindowList.count() > 1)
            {
            // determine the switch direction
            if( selected_window != effects->currentTabBoxWindow() )
                {
                if( selected_window != NULL )
                    {
                    int old_index = currentWindowList.indexOf( selected_window );
                    int new_index = effects->currentTabBoxWindowList().indexOf( effects->currentTabBoxWindow() );
                    Direction new_direction;
                    int distance = new_index - old_index;
                    if( distance > 0 )
                        new_direction = Left;
                    if( distance < 0 )
                        new_direction = Right;
                    if( effects->currentTabBoxWindowList().count() == 2 )
                        {
                        new_direction = Left;
                        distance = 1;
                        }
                    if( distance != 0 )
                        {
                        distance = abs( distance );
                        int tempDistance = effects->currentTabBoxWindowList().count() - distance;
                        if( tempDistance < abs( distance ) )
                            {
                            distance = tempDistance;
                            if( new_direction == Left )
                                new_direction = Right;
                            else
                                new_direction = Left;
                            }
                        if( !animation && !start )
                            {
                            animation = true;
                            direction = new_direction;
                            distance--;
                            }
                        for( int i=0; i<distance; i++ )
                            {
                            if( !scheduled_directions.isEmpty() && scheduled_directions.last() != new_direction )
                                scheduled_directions.pop_back();
                            else
                                scheduled_directions.enqueue( new_direction );
                            if( scheduled_directions.count() == effects->currentTabBoxWindowList().count() )
                                scheduled_directions.clear();
                            }
                        }
                    }
                selected_window = effects->currentTabBoxWindow();
                currentWindowList = effects->currentTabBoxWindowList();
                captionFrame.setText( selected_window->caption() );
                captionFrame.setIcon( selected_window->icon() );
                }
            }
            if( thumbnails && (!dynamicThumbnails || 
                (dynamicThumbnails && currentWindowList.size() >= thumbnailWindows)) )
                {
                calculateFrameSize();
                calculateItemSizes();
                }
        effects->addRepaintFull();
        }
    }

void CoverSwitchEffect::paintWindowCover( EffectWindow* w, bool reflectedWindow, WindowPaintData& data )
    {
    QRect windowRect = w->geometry();
    data.yTranslate = area.height() - windowRect.y() - windowRect.height();
    data.zTranslate = -zPosition;
    if( start )
        {
        if( w->isMinimized() )
            {
            data.opacity *= timeLine.value();
            }
        else
            {
            data.xTranslate *= timeLine.value();
            data.yTranslate *= timeLine.value();
            if( effects->numScreens() > 1)
                {
                QRect clientRect = effects->clientArea( FullScreenArea, w->screen(), effects->currentDesktop() );
                QRect fullRect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
                if( w->screen() == activeScreen )
                    {
                    if( clientRect.width() != fullRect.width() && clientRect.x() != fullRect.x() )
                        {
                        data.xTranslate -= clientRect.x()*(1.0f-timeLine.value());
                        }
                    if( clientRect.height() != fullRect.height() && clientRect.y() != fullRect.y() )
                        {
                        data.yTranslate -= clientRect.y()*(1.0f-timeLine.value());
                        }
                    }
                else
                    {
                    if( clientRect.width() != fullRect.width() && clientRect.x() < area.x())
                        {
                        data.xTranslate -= clientRect.width()*(1.0f-timeLine.value());
                        }
                    if( clientRect.height() != fullRect.height() && clientRect.y() < area.y() )
                        {
                        data.yTranslate -= clientRect.height()*(1.0f-timeLine.value());
                        }
                    }
                }
            data.zTranslate *= timeLine.value();
            if( data.rotation )
                data.rotation->angle *= timeLine.value();
            }
        }
    if( stop )
        {
        if( w->isMinimized() && w != effects->activeWindow() )
            {
            data.opacity *= (1.0 - timeLine.value());
            }
        else
            {
            data.xTranslate *= (1.0 - timeLine.value());
            data.yTranslate *= (1.0 - timeLine.value());
            if( effects->numScreens() > 1)
                {
                QRect clientRect = effects->clientArea( FullScreenArea, w->screen(), effects->currentDesktop() );
                QRect rect = effects->clientArea( FullScreenArea, activeScreen, effects->currentDesktop() );
                QRect fullRect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
                if( w->screen() == activeScreen )
                    {
                    if( clientRect.width() != fullRect.width() && clientRect.x() != fullRect.x() )
                        {
                        data.xTranslate -= clientRect.x()*timeLine.value();
                        }
                    if( clientRect.height() != fullRect.height() && clientRect.y() != fullRect.y() )
                        {
                        data.yTranslate -= clientRect.y()*timeLine.value();
                        }
                    }
                else
                    {
                    if( clientRect.width() != fullRect.width() && clientRect.x() < rect.x())
                        {
                        data.xTranslate -= clientRect.width()*timeLine.value();
                        }
                    if( clientRect.height() != fullRect.height() && clientRect.y() < area.y() )
                        {
                        data.yTranslate -= clientRect.height()*timeLine.value();
                        }
                    }
                }
            data.zTranslate *= (1.0 - timeLine.value());
            if( data.rotation )
                data.rotation->angle *= (1.0 - timeLine.value());
            }
        }

    QRect thumbnail = infiniteRegion();

    if( reflectedWindow )
        {
        glPushMatrix();
        glScalef( 1.0, -1.0, 1.0 );
        data.yTranslate = - area.height() - windowRect.y() - windowRect.height();
        effects->paintWindow( w,
            PAINT_WINDOW_TRANSFORMED,
            infiniteRegion(), data );
        glPopMatrix();
        }
    else
        {
        effects->paintWindow( w,
            PAINT_WINDOW_TRANSFORMED,
            infiniteRegion(), data );
        }
    }

void CoverSwitchEffect::paintFrontWindow( EffectWindow* frontWindow, int width, int leftWindows, int rightWindows, bool reflectedWindow )
    {
    if( frontWindow == NULL )
        return;
    float distance = 0.0;
    bool specialHandlingForward = false;
    WindowPaintData data( frontWindow );
    data.xTranslate = area.width()*0.5 - frontWindow->geometry().x() - frontWindow->geometry().width()*0.5;
    if( leftWindows == 0 )
        {
        leftWindows = 1;
        if( !start && !stop )
            specialHandlingForward = true;
        }
    if( rightWindows == 0 )
        {
        rightWindows = 1;
        }
    if( animation )
        {
        if( direction == Right )
            {
            // move to right
            distance = -frontWindow->geometry().width()*0.5f + area.width()*0.5f +
                (((float)displayWidth()*0.5*scaleFactor)-(float)area.width()*0.5f)/rightWindows;
            data.xTranslate += distance * timeLine.value();
            RotationData rot;
            rot.axis = RotationData::YAxis;
            rot.angle = -angle*timeLine.value();
            rot.xRotationPoint = frontWindow->geometry().width();
            data.rotation = &rot;
            }
        else
            {
            // move to left
            distance = frontWindow->geometry().width()*0.5f - area.width()*0.5f +
                ((float)width*0.5f-((float)displayWidth()*0.5*scaleFactor))/leftWindows;
            float factor = 1.0;
            if( specialHandlingForward )
                factor = 2.0;
            data.xTranslate += distance * timeLine.value() * factor;
            RotationData rot;
            rot.axis = RotationData::YAxis;
            rot.angle = angle*timeLine.value();
            data.rotation = &rot;
            }
        }
    if( specialHandlingForward )
        {
        data.opacity *= (1.0 - timeLine.value() * 2.0);
        paintWindowCover( frontWindow, reflectedWindow, data );
        }
    else
        paintWindowCover( frontWindow, reflectedWindow, data );
    }

void CoverSwitchEffect::paintWindows( const EffectWindowList& windows, bool left, bool reflectedWindows, EffectWindow* additionalWindow )
    {
    int width = area.width();
    int windowCount = windows.count();
    EffectWindow* window;
    
    int rotateFactor = 1;
    if( !left )
        {
        rotateFactor = -1;
        }

    float xTranslate = -((float)(width)*0.5f-((float)displayWidth()*0.5*scaleFactor));
    if( !left )
        xTranslate = ((float)displayWidth()*0.5*scaleFactor)-(float)width*0.5f;
    // handling for additional window from other side
    // has to appear on this side after half of the time
    if( animation && timeLine.value() >= 0.5 && additionalWindow != NULL )
        {
        RotationData rot;
        rot.axis = RotationData::YAxis;
        rot.angle = angle;
        rot.angle = angle*rotateFactor;
        WindowPaintData data( additionalWindow );
        if( left )
            data.xTranslate += -xTranslate - additionalWindow->geometry().x();
        else
            {
            data.xTranslate += xTranslate + area.width() - 
                additionalWindow->geometry().x() - additionalWindow->geometry().width();
            rot.xRotationPoint = additionalWindow->geometry().width();
            }
        data.rotation = &rot;
        data.opacity *= ( timeLine.value() - 0.5 ) * 2.0;
        paintWindowCover( additionalWindow, reflectedWindows, data );
        }
    RotationData rot;
    rot.axis = RotationData::YAxis;
    // normal behaviour
    for( int i=0; i < windows.count(); i++)
        {
        window = windows.at( i );
        if( window == NULL || window->isDeleted() )
            {
            continue;
            }
        WindowPaintData data( window );
        rot.angle = angle;
        if( left )
            data.xTranslate += -xTranslate + xTranslate*i/windowCount - window->geometry().x();
        else
            data.xTranslate += xTranslate + width - xTranslate*i/windowCount - window->geometry().x() - window->geometry().width();
        if( animation )
            {
            if( direction == Right )
                {
                if( ( i == windowCount - 1 ) && left )
                    {
                    // right most window on left side -> move to front
                    // have to move one window distance plus half the difference between the window and the desktop size
                    data.xTranslate += (xTranslate/windowCount + (width - window->geometry().width())*0.5f)*timeLine.value();
                    rot.angle = ( angle - angle * timeLine.value() );
                    }
                // right most window does not have to be moved
                else if( !left && ( i == 0 ) ); // do nothing
                else
                    {
                    // all other windows - move to next position
                    data.xTranslate += xTranslate/windowCount * timeLine.value();
                    }
                }
            else
                {
                if( ( i == windowCount - 1 ) && !left )
                    {
                    // left most window on right side -> move to front
                    data.xTranslate -= (xTranslate/windowCount + (width - window->geometry().width())*0.5f)*timeLine.value();
                    rot.angle = ( angle - angle * timeLine.value() );
                    }
                // left most window does not have to be moved
                else if( i==0 && left); // do nothing
                else
                    {
                    // all other windows - move to next position
                    data.xTranslate -= xTranslate/windowCount * timeLine.value();
                    }
                }
            }
        if( left )
            rot.xRotationPoint = 0.0;
        else
            rot.xRotationPoint = window->geometry().width();
        rot.angle *= rotateFactor;
        data.rotation = &rot;
        // make window most to edge transparent if animation
        if( animation && i == 0 && ( ( direction == Left && left ) || ( direction == Right && !left ) ) )
            {
            // only for the first half of the animation
            if( timeLine.value() < 0.5 )
                {
                data.opacity *= (1.0 - timeLine.value() * 2.0);
                paintWindowCover( window, reflectedWindows, data );
                }
            }
        else
            {
            paintWindowCover( window, reflectedWindows, data );
            }
        }
    }

// thumbnail bar - taken from boxswitch effect
void CoverSwitchEffect::calculateFrameSize()
    {
    int itemcount;

    QRect screenr = effects->clientArea( PlacementArea, activeScreen, effects->currentDesktop());
    itemcount = currentWindowList.count();
    item_max_size.setWidth( (screenr.width()*0.95f * 2)/itemcount );
    if( item_max_size.width() > 250 )
        item_max_size.setWidth( 250 );
    item_max_size.setHeight( item_max_size.width() * ((float)screenr.height()/(float)screenr.width()) );
    // Shrink the size until all windows/desktops can fit onscreen
    frame_area.setWidth( itemcount * item_max_size.width());
    frame_area.setHeight( item_max_size.height() );

    frame_area.moveTo( screenr.x() + ( screenr.width() - frame_area.width()) / 2,
        screenr.y() + screenr.height()*0.05f );

    thumbnailFrame.setGeometry( frame_area );
    }

void CoverSwitchEffect::calculateItemSizes()
    {
    qDeleteAll( windows );
    windows.clear();
    EffectWindowList original_windows = currentWindowList;
    int index = original_windows.indexOf( selected_window );
    int leftIndex = index;
    int rightIndex = index + 1;
    if( rightIndex == original_windows.count() )
        rightIndex = 0;
    EffectWindowList ordered_windows;

    int leftWindowCount = original_windows.count()/2;
    int rightWindowCount = leftWindowCount;
    if( original_windows.count()%2 == 1 )
        leftWindowCount++;
    for( int i=0; i < leftWindowCount; i++ )
        {
        int tempIndex = ( leftIndex - i );
        if( tempIndex < 0 )
            tempIndex = original_windows.count() + tempIndex;
        ordered_windows.prepend( original_windows[ tempIndex ] );
        }
    for( int i=0; i < rightWindowCount; i++ )
        {
        int tempIndex = ( rightIndex + i ) % original_windows.count();
        ordered_windows.append( original_windows[ tempIndex ] );
        }
    // move items cause of schedule
    for( int i=0; i < scheduled_directions.count(); i++ )
        {
        Direction actual = scheduled_directions[ i ];
        if( actual == Left )
            {
            EffectWindow* w = ordered_windows.takeLast();
            ordered_windows.prepend( w );
            }
        else
            {
            EffectWindow* w = ordered_windows.takeFirst();
            ordered_windows.append( w );
            }
        }
    if( animation && timeLine.value() < 0.5 )
        {
        if( direction == Left )
            {
            EffectWindow* w = ordered_windows.takeLast();
            edge_window = w;
            ordered_windows.prepend( w );
            }
        else
            {
            EffectWindow* w = ordered_windows.takeFirst();
            edge_window = w;
            ordered_windows.append( w );
            }
        }
    else if( animation && timeLine.value() >= 0.5 )
        {
        if( animation && direction == Left )
            edge_window = ordered_windows.last();
        if( animation && direction == Right )
            edge_window = ordered_windows.first();
        }
    int offset = 0;
    if( animation )
        {
        if( direction == Left )
            offset = (float)item_max_size.width()*(1.0-timeLine.value());
        else
            offset = -(float)item_max_size.width()*(1.0-timeLine.value());
        }
    for( int i = 0; i < ordered_windows.count(); i++ )
        {
        EffectWindow* w = ordered_windows.at( i );
        windows[ w ] = new ItemInfo();

        windows[ w ]->iconFrame = new EffectFrame( EffectFrame::Unstyled, false );
        windows[ w ]->iconFrame->setAlignment( Qt::AlignTop | Qt::AlignLeft );
        windows[ w ]->iconFrame->setIcon( w->icon() );

        float moveIndex = i;
        if( animation && timeLine.value() < 0.5 )
            {
            if( direction == Left )
                moveIndex--;
            else
                moveIndex++;
            }
        if( ordered_windows.count()%2 == 0 )
            moveIndex += 0.5;
        windows[ w ]->area = QRect( frame_area.x() + moveIndex * item_max_size.width() + offset,
            frame_area.y(),
            item_max_size.width(), item_max_size.height());
        windows[ w ]->clickable = windows[ w ]->area;
        }
    if( ordered_windows.count()%2 == 0 )
        {
        right_window = ordered_windows.last();
        }
    if( !highlight_is_set )
        {
        highlight_area = windows[ selected_window ]->area;
        highlight_is_set = true;
        }
    }

void CoverSwitchEffect::paintHighlight( QRect area )
    {
    glPushAttrib( GL_CURRENT_BIT );
    glColor4f( color_highlight.redF(), color_highlight.greenF(), color_highlight.blueF(), color_highlight.alphaF());
    renderRoundBox( area, 6 );
    glPopAttrib();
    }

void CoverSwitchEffect::paintWindowThumbnail( EffectWindow* w )
    {
    if( !windows.contains( w ))
        return;
    WindowPaintData data( w );

    setPositionTransformations( data,
        windows[ w ]->thumbnail, w,
        windows[ w ]->area.adjusted( highlight_margin, highlight_margin, -highlight_margin, -highlight_margin ),
        Qt::KeepAspectRatio );

    if( animation && ( w == edge_window ) && ( windows.size() % 2 == 1 ) )
        {
        // in case of animation:
        // the window which has to change the side will be split and painted on both sides of the edge
        double splitPoint = 0.0;
        if( direction == Left )
            {
            splitPoint = w->geometry().width()*timeLine.value();
            }
        else
            {
            splitPoint = w->geometry().width() - w->geometry().width()*timeLine.value();
            }
        data.quads = data.quads.splitAtX( splitPoint );
        WindowQuadList left_quads;
        WindowQuadList right_quads;
        foreach( const WindowQuad &quad, data.quads )
            {
            if( quad.left() >= splitPoint )
                left_quads << quad;
            if( quad.right() <= splitPoint )
                right_quads << quad;
            }
        // the base position of the window is changed after half of the animation
        if( timeLine.value() < 0.5 )
            {
            if( direction == Left )
                data.quads = left_quads;
            else
                data.quads = right_quads;
            }
        else
            {
            if( direction == Left )
                data.quads = right_quads;
            else
                data.quads = left_quads;
            }

        // paint one part of the thumbnail
        effects->drawWindow( w,
            PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSFORMED,
            windows[ w ]->thumbnail, data );

        QRect secondThumbnail;
        
        // paint the second part of the thumbnail:
        // the other window quads are needed and a new rect for transformation has to be calculated
        if( direction == Left )
            {
            if( timeLine.value() < 0.5 )
                {
                data.quads = right_quads;
                secondThumbnail = QRect( frame_area.x() + frame_area.width() - 
                    (float)item_max_size.width()*timeLine.value(),
                    frame_area.y(), item_max_size.width(), item_max_size.height());
                }
            else
                {
                data.quads = left_quads;
                secondThumbnail = QRect( frame_area.x() - (float)item_max_size.width()*timeLine.value(),
                    frame_area.y(), item_max_size.width(), item_max_size.height());
                if( right_window )
                    secondThumbnail = QRect( frame_area.x() -
                        (float)item_max_size.width()*(timeLine.value()-0.5),
                        frame_area.y(), item_max_size.width(), item_max_size.height());
                }
            }
        else
            {
            if( timeLine.value() < 0.5 )
                {
                data.quads = left_quads;
                secondThumbnail = QRect( frame_area.x() -
                    (float)item_max_size.width()*(1.0 - timeLine.value()),
                    frame_area.y(), item_max_size.width(), item_max_size.height());
                }
            else
                {
                data.quads = right_quads;
                secondThumbnail = QRect( frame_area.x() + frame_area.width() - 
                    (float)item_max_size.width()*(1.0 - timeLine.value()),
                    frame_area.y(), item_max_size.width(), item_max_size.height());
                }
            }
        setPositionTransformations( data,
            windows[ w ]->thumbnail, w,
            secondThumbnail.adjusted( highlight_margin, highlight_margin, -highlight_margin, -highlight_margin ),
            Qt::KeepAspectRatio );
        effects->drawWindow( w,
            PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSFORMED,
            windows[ w ]->thumbnail, data );
        }
    else if( ( windows.size() % 2 == 0 ) && ( w == right_window ) )
        {
        // in case of even number of thumbnails:
        // the window on the right is painted one half on left and on half on the right side
        float animationOffset = 0.0f;
        double splitPoint = w->geometry().width()*0.5;
        if( animation && timeLine.value() <= 0.5 )
            {
            // in case of animation the right window has only to be split during first half of animation
            if( direction == Left )
                {
                splitPoint += w->geometry().width()*timeLine.value();
                animationOffset = -(float)item_max_size.width()*timeLine.value();
                }
            else
                {
                splitPoint -= w->geometry().width()*timeLine.value();
                animationOffset = (float)item_max_size.width()*timeLine.value();
                }
            }
        if( animation && timeLine.value() > 0.5 )
            {
            // at half of animation a different window has become the right window
            // we have to adjust the splitting again
            if( direction == Left )
                {
                splitPoint -= w->geometry().width()*(1.0-timeLine.value());
                animationOffset = (float)item_max_size.width()*(1.0-timeLine.value());
                }
            else
                {
                splitPoint += w->geometry().width()*(1.0-timeLine.value());
                animationOffset = -(float)item_max_size.width()*(1.0-timeLine.value());
                }
            }
        data.quads = data.quads.splitAtX( splitPoint );
        WindowQuadList rightQuads;
        WindowQuadList leftQuads;
        foreach( const WindowQuad &quad, data.quads )
            {
            if( quad.right() <= splitPoint )
                leftQuads << quad;
            else
                rightQuads << quad;
            }

        // left quads are painted on right side of frame
        data.quads = leftQuads;
        effects->drawWindow( w,
            PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSFORMED,
            windows[ w ]->thumbnail, data );

        // right quads are painted on left side of frame
        data.quads = rightQuads;
        QRect secondThumbnail;
        secondThumbnail = QRect( frame_area.x() -
            (float)item_max_size.width()*0.5 + animationOffset,
            frame_area.y(), item_max_size.width(), item_max_size.height());
        setPositionTransformations( data,
            windows[ w ]->thumbnail, w,
            secondThumbnail.adjusted( highlight_margin, highlight_margin, -highlight_margin, -highlight_margin ),
            Qt::KeepAspectRatio );
        effects->drawWindow( w,
            PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSFORMED,
            windows[ w ]->thumbnail, data );        
        }
    else
        {
        effects->drawWindow( w,
            PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSFORMED,
            windows[ w ]->thumbnail, data );
        }
    }

void CoverSwitchEffect::paintWindowIcon( EffectWindow* w )
    {
    if( !windows.contains( w ))
        return;
    // Don't render null icons
    if( w->icon().isNull() )
        {
        return;
        }

    int width = w->icon().width();
    int height = w->icon().height();
    int x = windows[ w ]->area.x() + windows[ w ]->area.width() - width - highlight_margin;
    int y = windows[ w ]->area.y() + windows[ w ]->area.height() - height - highlight_margin;
    if( ( windows.size() % 2 == 0 ) )
        {
        if( w == right_window )
            {
            // in case of right window the icon has to be painted on the left side of the frame
            x = frame_area.x() + windows[ w ]->area.width()*0.5 - width - highlight_margin;
            if( animation )
                {
                if( timeLine.value() <= 0.5 )
                    {
                    if( direction == Left )
                        {
                        x -= windows[ w ]->area.width()*timeLine.value();
                        x = qMax( x, frame_area.x() );
                        }
                    else
                        x += windows[ w ]->area.width()*timeLine.value();
                    }
                else
                    {
                    if( direction == Left )
                        x += windows[ w ]->area.width()*(1.0-timeLine.value());
                    else
                        {
                        x -= windows[ w ]->area.width()*(1.0-timeLine.value());
                        x = qMax( x, frame_area.x() );
                        }
                    }
                }
            }
        }
    else
        {
        // during animation the icon of the edge window has to change position
        if( animation && w == edge_window )
            {
            if( timeLine.value() < 0.5 )
                {
                if( direction == Left )
                    x += windows[ w ]->area.width()*timeLine.value();
                else
                    x -= windows[ w ]->area.width()*timeLine.value();
                }
            else
                {
                if( direction == Left )
                    x -= windows[ w ]->area.width()*(1.0 - timeLine.value());
                else
                    x += windows[ w ]->area.width()*(1.0 - timeLine.value());
                }
            }
        }

    windows[ w ]->iconFrame->setPosition( QPoint( x, y ));
    windows[ w ]->iconFrame->render( infiniteRegion(), 1.0, 0.75 );
    }

void CoverSwitchEffect::windowInputMouseEvent( Window w, QEvent* e )
    {
    assert( w == input );
    if( e->type() != QEvent::MouseButtonPress )
        return;
    // we don't want click events during animations
    if( animation )
        return;
    QPoint pos = static_cast< QMouseEvent* >( e )->pos();

    // has one of the thumbnails been clicked?
    if( (thumbnails && (!dynamicThumbnails || 
        (dynamicThumbnails && currentWindowList.size() >= thumbnailWindows))))
        {
        // determine which item was clicked
        foreach( EffectWindow* w, windows.keys())
            {
            if( windows[ w ]->clickable.contains( pos ))
                {
                effects->setTabBoxWindow( w );
                return;
                }
            }
        // special handling for second half of window in case of animation and even number of windows
        if( windows.size() % 2 == 0 )
            {
            QRect additionalRect = QRect( frame_area.x(), frame_area.y(),
                item_max_size.width()*0.5, item_max_size.height());
            if( additionalRect.contains( pos ))
                {
                effects->setTabBoxWindow( right_window );
                return;
                }
            }
        }

    // determine if a window has been clicked
    // not interested in events above a fullscreen window (ignoring panel size)
    if( pos.y() < (area.height()*scaleFactor - area.height())*0.5f*(1.0f/scaleFactor) )
        return;

    if( pos.x() < (area.width()*scaleFactor - selected_window->width())*0.5f*(1.0f/scaleFactor) )
        {
        float availableSize = (area.width()*scaleFactor - area.width())*0.5f*(1.0f/scaleFactor);
        for( int i=0;i<leftWindows.count();i++ )
            {
            int windowPos = availableSize/leftWindows.count()*i;
            if( pos.x() < windowPos )
                continue;
            if( i+1 < leftWindows.count() )
                {
                if( pos.x() > availableSize/leftWindows.count()*(i+1) )
                    continue;
                }
            
            effects->setTabBoxWindow( leftWindows[i] );
            return;
            }
        }

    if( pos.x() > area.width() - (area.width()*scaleFactor - selected_window->width())*0.5f*(1.0f/scaleFactor) )
        {
        float availableSize = (area.width()*scaleFactor - area.width())*0.5f*(1.0f/scaleFactor);
        for( int i=0;i<rightWindows.count();i++ )
            {
            int windowPos = area.width() - availableSize/rightWindows.count()*i;
            if( pos.x() > windowPos )
                continue;
            if( i+1 < rightWindows.count() )
                {
                if( pos.x() < area.width() - availableSize/rightWindows.count()*(i+1) )
                    continue;
                }
            
            effects->setTabBoxWindow( rightWindows[i] );
            return;
            }
        }
    }

void CoverSwitchEffect::abort()
    {
    effects->unrefTabBox();
    effects->setActiveFullScreenEffect( 0 );
    effects->destroyInputWindow( input );
    mActivated = false;
    stop = false;
    stopRequested = false;
    effects->addRepaintFull();
    if( thumbnails && (!dynamicThumbnails ||
            (dynamicThumbnails && currentWindowList.size() >= thumbnailWindows)) )
        {
        qDeleteAll( windows );
        windows.clear();
        }
    captionFrame.free();
    thumbnailFrame.free();
    }

void CoverSwitchEffect::windowClosed( EffectWindow* c )
    {
    // if the list is not empty, the effect is active
    if( !currentWindowList.isEmpty() )
        {
        c->refWindow();
        referrencedWindows.append( c );
        currentWindowList.removeAll( c );
        leftWindows.removeAll( c );
        rightWindows.removeAll( c );
        }
    }


CoverSwitchEffect::ItemInfo::ItemInfo()
    : iconFrame( NULL )
    {
    }

CoverSwitchEffect::ItemInfo::~ItemInfo()
    {
    delete iconFrame;
    }

} // namespace
