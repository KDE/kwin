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

#include <kwinconfig.h>

#include "boxswitch.h"

#include <QCursor>
#include <QMouseEvent>
#include <QPainter>
#include <QSize>

#include <kapplication.h>
#include <kcolorscheme.h>

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
#include <GL/gl.h>
#endif

namespace KWin
{

KWIN_EFFECT( boxswitch, BoxSwitchEffect )

BoxSwitchEffect::BoxSwitchEffect()
    : mActivated( 0 )
    , mMode( 0 )
    , selected_window( 0 )
    , painting_desktop( 0 )
    {
    frame_margin = 10;
    highlight_margin = 5;
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    alphaFormat = XRenderFindStandardFormat( display(), PictStandardARGB32 );
#endif
    color_frame = KColorScheme( QPalette::Active, KColorScheme::Window ).background().color();
    color_frame.setAlphaF( 0.9 );
    color_highlight = KColorScheme( QPalette::Active, KColorScheme::Selection ).background().color();
    color_highlight.setAlphaF( 0.9 );
    color_text = KColorScheme( QPalette::Active, KColorScheme::Window ).foreground().color();
    }

BoxSwitchEffect::~BoxSwitchEffect()
    {
    }

void BoxSwitchEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( mActivated )
        {
        if( mMode == TabBoxWindowsMode )
            {
            if( windows.contains( w ))
                {
                if( w != selected_window )
                    data.setTranslucent();
                w->enablePainting( EffectWindow::PAINT_DISABLED_BY_MINIMIZE );
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

void BoxSwitchEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
    if( mActivated )
        {
        if( mMode == TabBoxWindowsMode )
            {
            paintFrame();

            foreach( EffectWindow* w, windows.keys())
                {
                if( w == selected_window )
                    {
                    paintHighlight( windows[ w ]->area );
                    }
                paintWindowThumbnail( w );
                paintWindowIcon( w );
                }
            paintText( selected_window->caption() );
            }
        else
            {
            if( !painting_desktop )
                {
                paintFrame();

                foreach( painting_desktop, desktops.keys())
                    {
                    if( painting_desktop == selected_desktop )
                        {
                        paintHighlight( desktops[ painting_desktop ]->area ); //effects->desktopName( painting_desktop )
                        }

                    paintDesktopThumbnail( painting_desktop );
                    }
                paintText( effects->desktopName( selected_desktop ));
                painting_desktop = 0;
                }
            }
        }
    }

void BoxSwitchEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( mActivated )
        {
        if( mMode == TabBoxWindowsMode )
            {
            if( windows.contains( w ) && w != selected_window )
                {
                data.opacity *= 0.2;
                }
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
        if( mMode == TabBoxWindowsMode )
            {
            if( selected_window != NULL )
                {
                if( windows.contains( selected_window ))
                    effects->addRepaint( windows.value( selected_window )->area );
                selected_window->addRepaintFull();
                }
            setSelectedWindow( effects->currentTabBoxWindow());
            if( windows.contains( selected_window ))
                effects->addRepaint( windows.value( selected_window )->area );
            selected_window->addRepaintFull();
            effects->addRepaint( text_area );
            if( effects->currentTabBoxWindowList() == original_windows )
                return;
            original_windows = effects->currentTabBoxWindowList();
            }
        else
            { // DesktopMode
            if( desktops.contains( selected_desktop ))
                effects->addRepaint( desktops.value( selected_desktop )->area );
            selected_desktop = effects->currentTabBoxDesktop();
            if( desktops.contains( selected_desktop ))
                effects->addRepaint( desktops.value( selected_desktop )->area );
            effects->addRepaint( text_area );
            if( effects->currentTabBoxDesktopList() == original_desktops )
                return;
            original_desktops = effects->currentTabBoxDesktopList();
            }
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
    if( mMode == TabBoxWindowsMode )
        {
        original_windows = effects->currentTabBoxWindowList();
        setSelectedWindow( effects->currentTabBoxWindow());
        }
    else
        {
        original_desktops = effects->currentTabBoxDesktopList();
        selected_desktop = effects->currentTabBoxDesktop();
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
        qDeleteAll( windows );
        windows.clear();
        setSelectedWindow( 0 );
        }
    else
        { // DesktopMode
        qDeleteAll( windows );
        desktops.clear();
        }
    effects->addRepaint( frame_area );
    frame_area = QRect();
    }

void BoxSwitchEffect::setSelectedWindow( EffectWindow* w )
    {
    if( selected_window )
        {
        effects->setElevatedWindow( selected_window, false );
        }
    selected_window = w;
    if( w )
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
    text_font.setBold( true );
    text_font.setPointSize( 12 );
    text_area.setHeight( QFontMetrics( text_font ).height() * 1.2 );
    // Separator space between items and text
    const int separator_height = 6;
    // Shrink the size until all windows/desktops can fit onscreen
    frame_area.setWidth( frame_margin * 2 + itemcount * item_max_size.width());
    QRect screenr = effects->clientArea( PlacementArea, effects->activeScreen(), effects->currentDesktop());
    while( frame_area.width() > screenr.width())
        {
        item_max_size /= 2;
        frame_area.setWidth( frame_margin * 2 + itemcount * item_max_size.width());
        }
    frame_area.setHeight( frame_margin * 2 + item_max_size.height() +
            separator_height + text_area.height());
    text_area.setWidth( frame_area.width() - frame_margin * 2 );

    frame_area.moveTo( screenr.x() + ( screenr.width() - frame_area.width()) / 2,
        screenr.y() + ( screenr.height() - frame_area.height()) / 2 );
    text_area.moveTo( frame_area.x() + frame_margin,
                      frame_area.y() + frame_margin + item_max_size.height() + separator_height);
    }

void BoxSwitchEffect::calculateItemSizes()
    {
    if( mMode == TabBoxWindowsMode )
        {
        windows.clear();
        for( int i = 0; i < original_windows.count(); i++ )
            {
            EffectWindow* w = original_windows.at( i );
            windows[ w ] = new ItemInfo();

            windows[ w ]->area = QRect( frame_area.x() + frame_margin
                + i * item_max_size.width(),
                frame_area.y() + frame_margin,
                item_max_size.width(), item_max_size.height());
            windows[ w ]->clickable = windows[ w ]->area;
            }
        }
    else
        {
        desktops.clear();
        for( int i = 0; i < original_desktops.count(); i++ )
            {
            int it = original_desktops.at( i );
            desktops[ it ] = new ItemInfo();

            desktops[ it ]->area = QRect( frame_area.x() + frame_margin
                + i * item_max_size.width(),
                frame_area.y() + frame_margin,
                item_max_size.width(), item_max_size.height());
            desktops[ it ]->clickable = desktops[ it ]->area;
            }
        }
    }

void BoxSwitchEffect::paintFrame()
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( effects->compositingType() == OpenGLCompositing )
        {
        glPushAttrib( GL_CURRENT_BIT );
        glColor4f( color_frame.redF(), color_frame.greenF(), color_frame.blueF(), color_frame.alphaF());
        renderRoundBoxWithEdge( frame_area );
        glPopAttrib();
        }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if( effects->compositingType() == XRenderCompositing )
        {
        Pixmap pixmap = XCreatePixmap( display(), rootWindow(),
            frame_area.width(), frame_area.height(), 32 );
        Picture pic = XRenderCreatePicture( display(), pixmap, alphaFormat, 0, NULL );
        XFreePixmap( display(), pixmap );
        XRenderColor col;
        col.alpha = int( color_frame.alphaF() * 0xffff );
        col.red = int( color_frame.redF() * color_frame.alphaF() * 0xffff );
        col.green = int( color_frame.greenF() * color_frame.alphaF() * 0xffff );
        col.blue = int( color_frame.blueF() * color_frame.alphaF() * 0xffff );
        XRenderFillRectangle( display(), PictOpSrc, pic, &col, 0, 0,
            frame_area.width(), frame_area.height());
        XRenderComposite( display(), color_frame.alphaF() != 1.0 ? PictOpOver : PictOpSrc,
            pic, None, effects->xrenderBufferPicture(),
            0, 0, 0, 0, frame_area.x(), frame_area.y(), frame_area.width(), frame_area.height());
        XRenderFreePicture( display(), pic );
        }
#endif
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
        Picture pic = XRenderCreatePicture( display(), pixmap, alphaFormat, 0, NULL );
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
        XRenderFreePicture( display(), pic );
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

    effects->drawWindow( w,
        PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSFORMED,
        windows[ w ]->thumbnail, data );
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

    if( windows[ w ]->icon.cacheKey() != w->icon().cacheKey())
        { // make sure windows[ w ]->icon is the right QPixmap, and rebind
        windows[ w ]->icon = w->icon();
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
        if( effects->compositingType() == OpenGLCompositing )
            {
            windows[ w ]->iconTexture.load( windows[ w ]->icon );
            windows[ w ]->iconTexture.setFilter( GL_LINEAR );
            }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        if( effects->compositingType() == XRenderCompositing )
            {
            windows[ w ]->iconPicture = XRenderCreatePicture( display(),
                windows[ w ]->icon.handle(), alphaFormat, 0, NULL );
            }
#endif
        }
    int width = windows[ w ]->icon.width();
    int height = windows[ w ]->icon.height();
    int x = windows[ w ]->area.x() + windows[ w ]->area.width() - width - highlight_margin;
    int y = windows[ w ]->area.y() + windows[ w ]->area.height() - height - highlight_margin;
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( effects->compositingType() == OpenGLCompositing )
        {
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        // Render some background
        glColor4f( 0, 0, 0, 0.5 );
        renderRoundBox( QRect( x-3, y-3, width+6, height+6 ), 3 );
        // Render the icon
        glColor4f( 1, 1, 1, 1 );
        windows[ w ]->iconTexture.bind();
        const float verts[ 4 * 2 ] =
            {
            x, y,
            x, y + height,
            x + width, y + height,
            x + width, y
            };
        const float texcoords[ 4 * 2 ] =
            {
            0, 1,
            0, 0,
            1, 0,
            1, 1
            };
        renderGLGeometry( 4, verts, texcoords );
        windows[ w ]->iconTexture.unbind();
        glPopAttrib();
        }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if( effects->compositingType() == XRenderCompositing )
        {
        XRenderComposite( display(),
            windows[ w ]->icon.depth() == 32 ? PictOpOver : PictOpSrc,
            windows[ w ]->iconPicture, None,
            effects->xrenderBufferPicture(),
            0, 0, 0, 0, x, y, width, height );
        }
#endif
    }

void BoxSwitchEffect::paintText( const QString& text )
{
    int maxwidth = text_area.width();
    effects->paintText( text, text_area.center(), maxwidth, color_text, text_font );
}

} // namespace
