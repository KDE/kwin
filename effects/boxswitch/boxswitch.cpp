/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>

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

#include "boxswitch.h"

#include <kwinconfig.h>

#include <QMouseEvent>
#include <QSize>

#include <kapplication.h>
#include <kcolorscheme.h>
#include <kconfiggroup.h>

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
#include <GL/gl.h>
#endif

namespace KWin
{

KWIN_EFFECT( boxswitch, BoxSwitchEffect )

BoxSwitchEffect::BoxSwitchEffect()
    : mActivated( 0 )
    , mMode( 0 )
    , thumbnailFrame( EffectFrame::Styled )
    , selected_window( 0 )
    , painting_desktop( 0 )
    , animation( false )
    , highlight_is_set( false )
    {
    text_font.setBold( true );
    text_font.setPointSize( 12 );
    thumbnailFrame.setFont( text_font );
    thumbnailFrame.setAlignment( Qt::AlignBottom | Qt::AlignHCenter );

    highlight_margin = 10;
    reconfigure( ReconfigureAll );
    }

BoxSwitchEffect::~BoxSwitchEffect()
    {
    }

void BoxSwitchEffect::reconfigure( ReconfigureFlags )
    {
    color_frame = KColorScheme( QPalette::Active, KColorScheme::Window ).background().color();
    color_frame.setAlphaF( 0.9 );
    color_highlight = KColorScheme( QPalette::Active, KColorScheme::Selection ).background().color();
    color_highlight.setAlphaF( 0.9 );
    activeTimeLine.setDuration( animationTime( 250 ));
    activeTimeLine.setCurveShape( TimeLine::EaseInOutCurve );
    timeLine.setDuration( animationTime( 150 ));
    timeLine.setCurveShape( TimeLine::EaseInOutCurve );
    KConfigGroup conf = effects->effectConfig( "BoxSwitch" );

    bg_opacity = conf.readEntry( "BackgroundOpacity", 25 ) / 100.0;
    elevate_window = conf.readEntry( "ElevateSelected", true );
    mAnimateSwitch = conf.readEntry( "AnimateSwitch", false );
    }

void BoxSwitchEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( activeTimeLine.value() != 0.0 )
        {
        if( mMode == TabBoxWindowsMode )
            {
            if( windows.contains( w ))
                {
                if( w != selected_window )
                    data.setTranslucent();
                w->enablePainting( EffectWindow::PAINT_DISABLED_BY_MINIMIZE | EffectWindow::PAINT_DISABLED_BY_DESKTOP );
                }
            }
        else
            {
            if( painting_desktop )
                {
                if( w->isOnDesktop( painting_desktop ))
                    w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
                else
                    w->disablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
                }
            }
        }
    effects->prePaintWindow( w, data, time );
    }

void BoxSwitchEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( mActivated )
        activeTimeLine.addTime( time );
    else
        {
        activeTimeLine.removeTime( time );
        if( activeTimeLine.value() == 0.0 )
            { // No longer need the window data
            qDeleteAll( windows );
            windows.clear();
            }
        }
    if( mActivated && animation )
        {
        timeLine.addTime( time );
        calculateItemSizes();
        }
    effects->prePaintScreen( data, time );
    }

void BoxSwitchEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
    if( mActivated )
        {
        if( mMode == TabBoxWindowsMode )
            {
            thumbnailFrame.render( region );

            if( mAnimateSwitch )
                {
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
            else
                {
                foreach( EffectWindow* w, windows.keys())
                    {
                    if( w == selected_window )
                        {
                        paintHighlight( windows[ w ]->area );
                        }
                    paintWindowThumbnail( w );
                    paintWindowIcon( w );
                    }
                }
            }
        else
            {
            if( !painting_desktop )
                {
                thumbnailFrame.render( region );

                foreach( painting_desktop, desktops.keys())
                    {
                    if( painting_desktop == selected_desktop )
                        {
                        paintHighlight( desktops[ painting_desktop ]->area ); //effects->desktopName( painting_desktop )
                        }

                    paintDesktopThumbnail( painting_desktop );
                    }
                painting_desktop = 0;
                }
            }
        }
    }

void BoxSwitchEffect::postPaintScreen()
    {
    if( mActivated && activeTimeLine.value() != 1.0 )
        effects->addRepaintFull();
    if( !mActivated && activeTimeLine.value() != 0.0 )
        effects->addRepaintFull();
    if( mActivated && animation )
        {
        if( timeLine.value() == 1.0 )
            {
            timeLine.setProgress( 0.0 );
            animation = false;
            if( !scheduled_directions.isEmpty() )
                {
                direction = scheduled_directions.dequeue();
                animation = true;
                }
            }
        QRect repaint = QRect( frame_area.x() - item_max_size.width()*0.5,
            frame_area.y(),
            frame_area.width() + item_max_size.width(),
            frame_area.height() );
        effects->addRepaint( repaint );
        }
    effects->postPaintScreen();
    }

void BoxSwitchEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if(( mActivated && mMode == TabBoxWindowsMode ) || ( !mActivated && activeTimeLine.value() != 0.0 ))
        {
        if( windows.contains( w ) && w != selected_window )
            {
            if( w->isMinimized() || !w->isOnCurrentDesktop() )
                // TODO: When deactivating minimized windows are not painted at all
                data.opacity *= activeTimeLine.value() * bg_opacity;
            else
                data.opacity *= 1.0 - activeTimeLine.value() * ( 1.0 - bg_opacity );
            }
        }
    effects->paintWindow( w, mask, region, data );
    }

void BoxSwitchEffect::windowInputMouseEvent( Window w, QEvent* e )
    {
    assert( w == mInput );
    if( e->type() != QEvent::MouseButtonPress )
        return;
    QPoint pos = static_cast< QMouseEvent* >( e )->pos();
    pos += frame_area.topLeft();

    // determine which item was clicked
    if( mMode == TabBoxWindowsMode )
        {
        foreach( EffectWindow* w, windows.keys())
            {
            if( windows[ w ]->clickable.contains( pos ))
                {
                effects->setTabBoxWindow( w );
                }
            }
        // special handling for second half of window in case of animation and even number of windows
        if( mAnimateSwitch && ( windows.size() % 2 == 0 ) )
            {
            QRect additionalRect = QRect( frame_area.x(), frame_area.y(),
                item_max_size.width()*0.5, item_max_size.height());
            if( additionalRect.contains( pos ))
                {
                effects->setTabBoxWindow( right_window );
                }
            }
        }
    else
        {
        foreach( int i, desktops.keys())
            {
            if( desktops[ i ]->clickable.contains( pos ))
                {
                effects->setTabBoxDesktop( i );
                }
            }
        }
    }

void BoxSwitchEffect::windowDamaged( EffectWindow* w, const QRect& damage )
    {
    if( mActivated )
        {
        if( mMode == TabBoxWindowsMode )
            {
            if( windows.contains( w ))
                {
                effects->addRepaint( windows[ w ]->area );
                }
            }
        else
            {
            if( w->isOnAllDesktops())
                {
                foreach( ItemInfo* info, desktops )
                    effects->addRepaint( info->area );
                }
            else
                {
                effects->addRepaint( desktops[ w->desktop() ]->area );
                }
            }
        }
    }

void BoxSwitchEffect::windowGeometryShapeChanged( EffectWindow* w, const QRect& old )
    {
    if( mActivated )
        {
        if( mMode == TabBoxWindowsMode )
            {
            if( windows.contains( w ) && w->size() != old.size())
                {
                effects->addRepaint( windows[ w ]->area );
                }
            }
        else
            {
            if( w->isOnAllDesktops())
                {
                foreach( ItemInfo* info, desktops )
                    effects->addRepaint( info->area );
                }
            else
                {
                effects->addRepaint( desktops[ w->desktop() ]->area );
                }
            }
        }
    }

void BoxSwitchEffect::tabBoxAdded( int mode )
    {
    if( !mActivated )
        {
        if( mode == TabBoxWindowsMode )
            {
            if( effects->currentTabBoxWindowList().count() > 0 )
                {
                mMode = mode;
                effects->refTabBox();
                highlight_is_set = false;
                animation = false;
                scheduled_directions.clear();
                right_window = 0;
                setActive();
                }
            }
        else
            { // DesktopMode
            if( effects->currentTabBoxDesktopList().count() > 0 )
                {
                mMode = mode;
                painting_desktop = 0;
                effects->refTabBox();
                setActive();
                }
            }
        }
    }

void BoxSwitchEffect::tabBoxClosed()
    {
    if( mActivated )
        setInactive();
    }

void BoxSwitchEffect::tabBoxUpdated()
    {
    if( mActivated )
        {
        if( mMode == TabBoxWindowsMode && selected_window != effects->currentTabBoxWindow() )
            {
            if( selected_window != NULL )
                {
                if( mAnimateSwitch )
                    {
                    int old_index = effects->currentTabBoxWindowList().indexOf( selected_window );
                    int new_index = effects->currentTabBoxWindowList().indexOf( effects->currentTabBoxWindow() );
                    Direction new_direction;
                    int distance = new_index - old_index;
                    if( distance > 0 )
                        new_direction = Left;
                    if( distance < 0 )
                        new_direction = Right;
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
                        if( !animation )
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
                if( windows.contains( selected_window ))
                    effects->addRepaint( windows.value( selected_window )->area );
                selected_window->addRepaintFull();
                }
            setSelectedWindow( effects->currentTabBoxWindow());
            if( windows.contains( selected_window ))
                effects->addRepaint( windows.value( selected_window )->area );
            selected_window->addRepaintFull();
            effects->addRepaint( text_area );
            }
        else if( mMode != TabBoxWindowsMode )
            { // DesktopMode
            if( desktops.contains( selected_desktop ))
                effects->addRepaint( desktops.value( selected_desktop )->area );
            selected_desktop = effects->currentTabBoxDesktop();
            thumbnailFrame.setText( effects->desktopName( selected_desktop ));
            if( desktops.contains( selected_desktop ))
                effects->addRepaint( desktops.value( selected_desktop )->area );
            effects->addRepaint( text_area );
            if( effects->currentTabBoxDesktopList() == original_desktops )
                return;
            original_desktops = effects->currentTabBoxDesktopList();
            }
        // for safety copy list each time tabbox is updated
        original_windows = effects->currentTabBoxWindowList();
        effects->addRepaint( frame_area );
        calculateFrameSize();
        calculateItemSizes();
        moveResizeInputWindow( frame_area.x(), frame_area.y(), frame_area.width(), frame_area.height());
        effects->addRepaint( frame_area );
        }
    }

void BoxSwitchEffect::setActive()
    {
    mActivated = true;

    // Just in case we are activated again before the deactivation animation finished
    qDeleteAll( windows );
    windows.clear();

    if( mMode == TabBoxWindowsMode )
        {
        original_windows = effects->currentTabBoxWindowList();
        setSelectedWindow( effects->currentTabBoxWindow());
        }
    else
        {
        original_desktops = effects->currentTabBoxDesktopList();
        selected_desktop = effects->currentTabBoxDesktop();
        thumbnailFrame.setText( effects->desktopName( selected_desktop ));
        }
    calculateFrameSize();
    calculateItemSizes();
    mInput = effects->createInputWindow( this, frame_area.x(), frame_area.y(),
        frame_area.width(), frame_area.height(), Qt::ArrowCursor );
    effects->addRepaint( frame_area );
    if( mMode == TabBoxWindowsMode )
        {
        foreach( EffectWindow* w, windows.keys())
            {
            w->addRepaintFull();
            }
        }
    }

void BoxSwitchEffect::setInactive()
    {
    mActivated = false;
    effects->unrefTabBox();
    if( mInput != None )
        {
        effects->destroyInputWindow( mInput );
        mInput = None;
        }
    if( mMode == TabBoxWindowsMode )
        {
        foreach( EffectWindow* w, windows.keys())
            {
            if( w != selected_window )
                w->addRepaintFull();
            }
        // We don't unset the selected window so we have correct fading
        // But we do need to remove elevation status
        if( elevate_window && selected_window )
            effects->setElevatedWindow( selected_window, false );

        foreach( EffectWindow* w, referrencedWindows )
            {
            w->unrefWindow();
            }
        referrencedWindows.clear();
        }
    else
        { // DesktopMode
        qDeleteAll( windows );
        desktops.clear();
        }
    thumbnailFrame.free();
    effects->addRepaint( frame_area );
    frame_area = QRect();
    }

void BoxSwitchEffect::setSelectedWindow( EffectWindow* w )
    {
    if( elevate_window && selected_window )
        {
        effects->setElevatedWindow( selected_window, false );
        }
    selected_window = w;
    if( selected_window )
        thumbnailFrame.setText( selected_window->caption() );
    if( elevate_window && w )
        {
        effects->setElevatedWindow( selected_window, true );
        }
    }

void BoxSwitchEffect::windowClosed( EffectWindow* w )
    {
    if( w == selected_window )
        {
        setSelectedWindow( 0 );
        }
    if( windows.contains( w ) )
        {
        w->refWindow();
        referrencedWindows.append( w );
        original_windows.removeAll( w );
        delete windows[ w ];
        windows.remove( w );
        effects->addRepaintFull();
        }
    }

void BoxSwitchEffect::moveResizeInputWindow( int x, int y, int width, int height )
    {
    XMoveWindow( display(), mInput, x, y );
    XResizeWindow( display(), mInput, width, height );
    }

void BoxSwitchEffect::calculateFrameSize()
    {
    int itemcount;

    if( mMode == TabBoxWindowsMode )
        {
        itemcount = original_windows.count();
        item_max_size.setWidth( 200 );
        item_max_size.setHeight( 200 );
        }
    else
        {
        itemcount = original_desktops.count();
        item_max_size.setWidth( 200 );
        item_max_size.setHeight( 200 );
        }
    // How much height to reserve for a one-line text label
    text_area.setHeight( QFontMetrics( text_font ).height() * 1.2 );
    // Separator space between items and text
    const int separator_height = 6;
    // Shrink the size until all windows/desktops can fit onscreen
    frame_area.setWidth( itemcount * item_max_size.width());
    QRect screenr = effects->clientArea( PlacementArea, effects->activeScreen(), effects->currentDesktop());
    while( frame_area.width() > screenr.width())
        {
        item_max_size /= 2;
        frame_area.setWidth( itemcount * item_max_size.width());
        }
    frame_area.setHeight( item_max_size.height() +
            separator_height + text_area.height());
    text_area.setWidth( frame_area.width() );

    frame_area.moveTo( screenr.x() + ( screenr.width() - frame_area.width()) / 2,
        screenr.y() + ( screenr.height() - frame_area.height()) / 2 );
    text_area.moveTo( frame_area.x(),
                      frame_area.y() + item_max_size.height() + separator_height);

    thumbnailFrame.setGeometry( frame_area );
    }

void BoxSwitchEffect::calculateItemSizes()
    {
    if( mMode == TabBoxWindowsMode )
        {
        qDeleteAll( windows );
        windows.clear();
        if( mAnimateSwitch )
            {
            int index = original_windows.indexOf( effects->currentTabBoxWindow() );
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
                if( !ordered_windows.at( i ) )
                    continue;
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
        else
            {
            for( int i = 0; i < original_windows.count(); i++ )
                {
                if( !original_windows.at( i ) )
                    continue;
                EffectWindow* w = original_windows.at( i );
                windows[ w ] = new ItemInfo();

                windows[ w ]->iconFrame = new EffectFrame( EffectFrame::Unstyled, false );
                windows[ w ]->iconFrame->setAlignment( Qt::AlignTop | Qt::AlignLeft );
                windows[ w ]->iconFrame->setIcon( w->icon() );

                windows[ w ]->area = QRect( frame_area.x() + i * item_max_size.width(),
                    frame_area.y(),
                    item_max_size.width(), item_max_size.height());
                windows[ w ]->clickable = windows[ w ]->area;
                }
            }
        }
    else
        {
        desktops.clear();
        for( int i = 0; i < original_desktops.count(); i++ )
            {
            int it = original_desktops.at( i );
            desktops[ it ] = new ItemInfo();

            desktops[ it ]->area = QRect( frame_area.x() + i * item_max_size.width(),
                frame_area.y(),
                item_max_size.width(), item_max_size.height());
            desktops[ it ]->clickable = desktops[ it ]->area;
            }
        }
    }

void BoxSwitchEffect::paintHighlight( QRect area )
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( effects->compositingType() == OpenGLCompositing )
        {
        glPushAttrib( GL_CURRENT_BIT );
        glColor4f( color_highlight.redF(), color_highlight.greenF(), color_highlight.blueF(), color_highlight.alphaF());
        renderRoundBox( area, 6 );
        glPopAttrib();
        }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if( effects->compositingType() == XRenderCompositing )
        {
        Pixmap pixmap = XCreatePixmap( display(), rootWindow(),
            area.width(), area.height(), 32 );
        XRenderPicture pic( pixmap, 32 );
        XFreePixmap( display(), pixmap );
        XRenderColor col;
        col.alpha = int( color_highlight.alphaF() * 0xffff );
        col.red = int( color_highlight.redF() * color_highlight.alphaF() * 0xffff );
        col.green = int( color_highlight.greenF() * color_highlight.alphaF() * 0xffff );
        col.blue = int( color_highlight.blueF() * color_highlight.alphaF() * 0xffff );
        XRenderFillRectangle( display(), PictOpSrc, pic, &col, 0, 0,
            area.width(), area.height());
        XRenderComposite( display(), color_highlight.alphaF() != 1.0 ? PictOpOver : PictOpSrc,
            pic, None, effects->xrenderBufferPicture(),
            0, 0, 0, 0, area.x(), area.y(), area.width(), area.height());
        }
#endif
    }

void BoxSwitchEffect::paintWindowThumbnail( EffectWindow* w )
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

void BoxSwitchEffect::paintDesktopThumbnail( int iDesktop )
    {
    if( !desktops.contains( iDesktop ))
        return;

    ScreenPaintData data;
    QRect region;
    QRect r = desktops[ iDesktop ]->area.adjusted( highlight_margin, highlight_margin,
        -highlight_margin, -highlight_margin );
    QSize size = QSize( displayWidth(), displayHeight());

    size.scale( r.size(), Qt::KeepAspectRatio );
    data.xScale = size.width() / double( displayWidth());
    data.yScale = size.height() / double( displayHeight());
    int width = int( displayWidth() * data.xScale );
    int height = int( displayHeight() * data.yScale );
    int x = r.x() + ( r.width() - width ) / 2;
    int y = r.y() + ( r.height() - height ) / 2;
    region = QRect( x, y, width, height );
    data.xTranslate = x;
    data.yTranslate = y;

    effects->paintScreen( PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_BACKGROUND_FIRST,
        region, data );
    }

void BoxSwitchEffect::paintWindowIcon( EffectWindow* w )
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

BoxSwitchEffect::ItemInfo::ItemInfo()
    : iconFrame( NULL )
    {
    }

BoxSwitchEffect::ItemInfo::~ItemInfo()
    {
    delete iconFrame;
    }

} // namespace
