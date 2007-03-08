/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "boxswitch.h"

#include <tabbox.h>
#include <client.h>
#include <scene_xrender.h>
#include <scene_opengl.h>
#include <QSize>

#ifdef HAVE_OPENGL
#include <GL/gl.h>
#endif

namespace KWinInternal
{

BoxSwitchEffect::BoxSwitchEffect()
    : mActivated( 0 )
    , mMode( 0 )
    , painting_desktop( 0 )
    {
    frame_margin = 10;
    highlight_margin = 5;
#ifdef HAVE_XRENDER
    alphaFormat = XRenderFindStandardFormat( display(), PictStandardARGB32 );
#endif
    }

BoxSwitchEffect::~BoxSwitchEffect()
    {
    }

void BoxSwitchEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    effects->prePaintScreen( mask, region, time );
    }

void BoxSwitchEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time )
    {
    if( mActivated )
        {
        if( mMode == TabBox::WindowsMode )
            {
            if( windows.contains( w ) && w != selected_window )
                {
                *mask |= Scene::PAINT_WINDOW_TRANSLUCENT;
                *mask &= ~Scene::PAINT_WINDOW_OPAQUE;
                }
            }
        else
            {
            if( painting_desktop )
                {
                if( w->isOnDesktop( painting_desktop ))
                    w->enablePainting( Scene::Window::PAINT_DISABLED_BY_DESKTOP );
                else
                    w->disablePainting( Scene::Window::PAINT_DISABLED_BY_DESKTOP );
                }
            }
        }
    effects->prePaintWindow( w, mask, region, time );
    }

void BoxSwitchEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
    if( mActivated )
        {
        if( mMode == TabBox::WindowsMode )
            {
            paintFrame();

            foreach( EffectWindow* w, windows.keys())
                {
                if( w == selected_window )
                    {
                    paintHighlight( windows[ w ].area,
                        static_cast< Client* >( w->window())->caption());
                    }
                paintWindowThumbnail( w );
                paintWindowIcon( w );
                }
            }
        else
            {
            if( !painting_desktop )
                {
                paintFrame();

                for( painting_desktop = 1; painting_desktop <= desktops.count(); painting_desktop++ )
                    {
                    if( painting_desktop == selected_desktop )
                        {
                        paintHighlight( desktops[ painting_desktop ].area,
                            Workspace::self()->desktopName( painting_desktop ));
                        }

                    paintDesktopThumbnail( painting_desktop );
                    }
                painting_desktop = 0;
                }
            }
        }
    }

void BoxSwitchEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( mActivated )
        {
        if( mMode == TabBox::WindowsMode )
            {
            if( windows.contains( w ) && w != selected_window )
                {
                data.opacity *= 0.1;
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
    if( mMode == TabBox::WindowsMode )
        {
        foreach( EffectWindow* w, windows.keys())
            {
            if( windows[ w ].clickable.contains( pos ))
                {
                if( Client* c = static_cast< Client* >( w->window()))
                    {
                    Workspace::self()->activateClient( c );
                    if( c->isShade() && options->shadeHover )
                        c->setShade( ShadeActivated );
                    Workspace::self()->unrefTabBox();
                    setInactive();
                    }
                }
            }
        }
    else
        {
        for( int i = 1; i <= desktops.count(); i++ )
            {
            if( desktops[ i ].clickable.contains( pos ))
                {
                Workspace::self()->setCurrentDesktop( i );
                Workspace::self()->unrefTabBox();
                setInactive();
                }
            }
        }
    }

void BoxSwitchEffect::windowDamaged( EffectWindow* w, const QRect& damage )
    {
    if( mActivated )
        {
        if( mMode == TabBox::WindowsMode )
            {
            if( windows.contains( w ))
                {
                workspace()->addRepaint( windows[ w ].area );
                }
            }
        else
            {
            if( w->isOnAllDesktops())
                {
                foreach( ItemInfo info, desktops )
                    workspace()->addRepaint( info.area );
                }
            else
                {
                workspace()->addRepaint( desktops[ w->desktop() ].area );
                }
            }
        }
    }

void BoxSwitchEffect::windowGeometryShapeChanged( EffectWindow* w, const QRect& old )
    {
    if( mActivated )
        {
        if( mMode == TabBox::WindowsMode )
            {
            if( windows.contains( w ) && w->size() != old.size())
                {
                workspace()->addRepaint( windows[ w ].area );
                }
            }
        else
            {
            if( w->isOnAllDesktops())
                {
                foreach( ItemInfo info, desktops )
                    workspace()->addRepaint( info.area );
                }
            else
                {
                workspace()->addRepaint( desktops[ w->desktop() ].area );
                }
            }
        }
    }

void BoxSwitchEffect::tabBoxAdded( int mode )
    {
    if( !mActivated )
        {
        if( mode == TabBox::WindowsMode )
            {
            if( Workspace::self()->currentTabBoxClientList().count() > 1 )
                {
                mMode = mode;
                Workspace::self()->refTabBox();
                setActive();
                }
            }
        else
            { // DesktopMode or DesktopListMode
            if( Workspace::self()->numberOfDesktops() > 1 )
                {
                mMode = mode;
                painting_desktop = 0;
                Workspace::self()->refTabBox();
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
        if( mMode == TabBox::WindowsMode )
            {
            if( selected_window != NULL )
                {
                if( windows.contains( selected_window ))
                    workspace()->addRepaint( windows.value( selected_window ).area );
                selected_window->window()->addRepaintFull();
                }
            selected_window = Workspace::self()->currentTabBoxClient()->effectWindow();
            if( windows.contains( selected_window ))
                workspace()->addRepaint( windows.value( selected_window ).area );
            selected_window->window()->addRepaintFull();
            if( Workspace::self()->currentTabBoxClientList() == tab_clients )
                return;
            tab_clients = Workspace::self()->currentTabBoxClientList();
            }
        else
            {
            if( desktops.contains( selected_desktop ))
                workspace()->addRepaint( desktops.value( selected_desktop ).area );
            selected_desktop = Workspace::self()->currentTabBoxDesktop();
            if( desktops.contains( selected_desktop ))
                workspace()->addRepaint( desktops.value( selected_desktop ).area );
            if( Workspace::self()->numberOfDesktops() == desktops.count())
                return;
            }
        workspace()->addRepaint( frame_area );
        calculateFrameSize();
        calculateItemSizes();
        moveResizeInputWindow( frame_area.x(), frame_area.y(), frame_area.width(), frame_area.height());
        workspace()->addRepaint( frame_area );
        }
    }

void BoxSwitchEffect::setActive()
    {
    mActivated = true;
    if( mMode == TabBox::WindowsMode )
        {
        tab_clients = Workspace::self()->currentTabBoxClientList();
        selected_window = Workspace::self()->currentTabBoxClient()->effectWindow();
        }
    else
        {
        selected_desktop = Workspace::self()->currentTabBoxDesktop();
        }
    calculateFrameSize();
    calculateItemSizes();
    mInput = effects->createInputWindow( this, frame_area.x(), frame_area.y(),
        frame_area.width(), frame_area.height(), Qt::ArrowCursor );
    workspace()->addRepaint( frame_area );
    if( mMode == TabBox::WindowsMode )
        {
        foreach( EffectWindow* w, windows.keys())
            {
            if( w != selected_window )
                w->window()->addRepaintFull();
#ifdef HAVE_OPENGL
            if( dynamic_cast< SceneOpenGL* >( scene ))
                {
//                windows[ w ].iconTexture.discard();
                }
            else
#endif
                {
#ifdef HAVE_XRENDER
            if( windows[ w ].iconPicture != None )
                XRenderFreePicture( display(), windows[ w ].iconPicture );
            windows[ w ].iconPicture = None;
#endif
                }
            }
        }
    }

void BoxSwitchEffect::setInactive()
    {
    mActivated = false;
    if( mInput != None )
        {
        effects->destroyInputWindow( mInput );
        mInput = None;
        }
    if( mMode == TabBox::WindowsMode )
        {
        foreach( EffectWindow* w, windows.keys())
            {
            if( w != selected_window )
                w->window()->addRepaintFull();
#ifdef HAVE_OPENGL
            if( dynamic_cast< SceneOpenGL* >( scene ))
                {
//                windows[ w ].iconTexture.discard();
                }
            else
#endif
                {
#ifdef HAVE_XRENDER
            if( windows[ w ].iconPicture != None )
                XRenderFreePicture( display(), windows[ w ].iconPicture );
            windows[ w ].iconPicture = None;
#endif
                }
            }
        }
    workspace()->addRepaint( frame_area );
    frame_area = QRect();
    }

void BoxSwitchEffect::moveResizeInputWindow( int x, int y, int width, int height )
    {
    XMoveWindow( display(), mInput, x, y );
    XResizeWindow( display(), mInput, width, height );
    }

void BoxSwitchEffect::calculateFrameSize()
    {
    int itemcount;

    if( mMode == TabBox::WindowsMode )
        {
        itemcount = tab_clients.count();
        item_max_size.setWidth( 200 );
        item_max_size.setHeight( 200 );
        }
    else
        {
        itemcount = Workspace::self()->numberOfDesktops();
        item_max_size.setWidth( 200 );
        item_max_size.setHeight( 200 );
        }
    // Shrink the size until all windows/desktops can fit onscreen
    frame_area.setWidth( frame_margin * 2 + itemcount * item_max_size.width());
    while( frame_area.width() > displayWidth())
        {
        item_max_size /= 2;
        frame_area.setWidth( frame_margin * 2 + itemcount * item_max_size.width());
        }
    frame_area.setHeight( frame_margin * 2 + item_max_size.height() + 15 );
    frame_area.moveTo( ( displayWidth() - frame_area.width()) / 2, ( displayHeight() - frame_area.height()) / 2 );
    }

void BoxSwitchEffect::calculateItemSizes()
    {
    if( mMode == TabBox::WindowsMode )
        {
        windows.clear();
        for( int i = 0; i < tab_clients.count(); i++ )
            {
            EffectWindow* w = tab_clients.at( i )->effectWindow();

            windows[ w ].area = QRect( frame_area.x() + frame_margin
                + i * item_max_size.width(),
                frame_area.y() + frame_margin,
                item_max_size.width(), item_max_size.height());
            windows[ w ].clickable = windows[ w ].area;
            }
        }
    else
        {
        desktops.clear();
        int iDesktop = ( mMode == TabBox::DesktopMode ) ? Workspace::self()->currentDesktop() : 1;
        for( int i = 1; i <= Workspace::self()->numberOfDesktops(); i++ )
            {
            desktops[ iDesktop ].area = QRect( frame_area.x() + frame_margin
                + ( i - 1 ) * item_max_size.width(),
                frame_area.y() + frame_margin,
                item_max_size.width(), item_max_size.height());
            desktops[ iDesktop ].clickable = desktops[ iDesktop ].area;

            if( mMode == TabBox::DesktopMode )
                iDesktop = Workspace::self()->nextDesktopFocusChain( iDesktop );
            else
                iDesktop++;
            }
        }
    }

void BoxSwitchEffect::paintFrame()
    {
    double alpha = 0.75;
#ifdef HAVE_OPENGL
    if( dynamic_cast< SceneOpenGL* >( scene ))
        {
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        glColor4f( 0, 0, 0, alpha );
        glBegin( GL_QUADS );
        glVertex2i( frame_area.x(), frame_area.y());
        glVertex2i( frame_area.x() + frame_area.width(), frame_area.y());
        glVertex2i( frame_area.x() + frame_area.width(), frame_area.y() + frame_area.height());
        glVertex2i( frame_area.x(), frame_area.y() + frame_area.height());
        glEnd();
        glPopAttrib();
        }
    else
#endif
        {
#ifdef HAVE_XRENDER
        Pixmap pixmap = XCreatePixmap( display(), rootWindow(),
            frame_area.width(), frame_area.height(), 32 );
        Picture pic = XRenderCreatePicture( display(), pixmap, alphaFormat, 0, NULL );
        XFreePixmap( display(), pixmap );
        XRenderColor col;
        col.alpha = int( alpha * 0xffff );
        col.red = 0;
        col.green = 0;
        col.blue = 0;
        XRenderFillRectangle( display(), PictOpSrc, pic, &col, 0, 0,
            frame_area.width(), frame_area.height());
        XRenderComposite( display(), alpha != 1.0 ? PictOpOver : PictOpSrc,
            pic, None, static_cast< SceneXrender* >( scene )->bufferPicture(),
            0, 0, 0, 0, frame_area.x(), frame_area.y(), frame_area.width(), frame_area.height());
        XRenderFreePicture( display(), pic );
#endif
        }
    }

void BoxSwitchEffect::paintHighlight( QRect area, QString text )
    {
    double alpha = 0.75;
#ifdef HAVE_OPENGL
    if( dynamic_cast< SceneOpenGL* >( scene ))
        {
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        glColor4f( 1, 1, 1, alpha );
        glBegin( GL_QUADS );
        glVertex2i( area.x(), area.y());
        glVertex2i( area.x() + area.width(), area.y());
        glVertex2i( area.x() + area.width(), area.y() + area.height());
        glVertex2i( area.x(), area.y() + area.height());
        glEnd();
        glPopAttrib();
        }
    else
#endif
        {
#ifdef HAVE_XRENDER
        Pixmap pixmap = XCreatePixmap( display(), rootWindow(),
            area.width(), area.height(), 32 );
        Picture pic = XRenderCreatePicture( display(), pixmap, alphaFormat, 0, NULL );
        XFreePixmap( display(), pixmap );
        XRenderColor col;
        col.alpha = int( alpha * 0xffff );
        col.red = int( alpha * 0xffff );
        col.green = int( alpha * 0xffff );
        col.blue = int( alpha * 0xffff );
        XRenderFillRectangle( display(), PictOpSrc, pic, &col, 0, 0,
            area.width(), area.height());
        XRenderComposite( display(), alpha != 1.0 ? PictOpOver : PictOpSrc,
            pic, None, static_cast< SceneXrender* >( scene )->bufferPicture(),
            0, 0, 0, 0, area.x(), area.y(), area.width(), area.height());
        XRenderFreePicture( display(), pic );
#endif
        }
//    kDebug() << text << endl; // TODO draw this nicely on screen
    }

void BoxSwitchEffect::paintDesktopThumbnail( int iDesktop )
    {
    if( !desktops.contains( iDesktop ))
        return;

    ScreenPaintData data;
    QRect region;
    QRect r = desktops[ iDesktop ].area.adjusted( highlight_margin, highlight_margin,
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

    effects->paintScreen( Scene::PAINT_SCREEN_TRANSFORMED | Scene::PAINT_SCREEN_BACKGROUND_FIRST,
        region, data );
    }

void BoxSwitchEffect::paintWindowThumbnail( EffectWindow* w )
    {
    if( !windows.contains( w ))
        return;
    WindowPaintData data;

    setPositionTransformations( data,
        windows[ w ].thumbnail, w,
        windows[ w ].area.adjusted( highlight_margin, highlight_margin, -highlight_margin, -highlight_margin ),
        Qt::KeepAspectRatio );

    effects->drawWindow( w,
        Scene::PAINT_WINDOW_OPAQUE | Scene::PAINT_WINDOW_TRANSFORMED,
        windows[ w ].thumbnail, data );
    }

void BoxSwitchEffect::paintWindowIcon( EffectWindow* w )
    {
    if( !windows.contains( w ))
        return;
    if( windows[ w ].icon.serialNumber()
        != static_cast< Client* >( w->window())->icon().serialNumber())
        { // make sure windows[ w ].icon is the right QPixmap, and rebind
        windows[ w ].icon = static_cast< Client* >( w->window())->icon();
#ifdef HAVE_OPENGL
        if( dynamic_cast< SceneOpenGL* >( scene ))
            {
//            windows[ w ].iconTexture.bindPixmap( windows[ w ].icon.handle(),
//                windows[ w ].icon.width(), windows[ w ].icon.height(),
//                windows[ w ].icon.depth(), windows[ w ].icon.rect());
//            windows[ w ].iconTexture.setFilter( GL_LINEAR );
            }
        else
#endif
            {
#ifdef HAVE_XRENDER
            if( windows[ w ].iconPicture != None )
                XRenderFreePicture( display(), windows[ w ].iconPicture );
            windows[ w ].iconPicture = XRenderCreatePicture( display(),
                windows[ w ].icon.handle(), alphaFormat, 0, NULL );
#endif
            }
        }
    int x = windows[ w ].area.x() + ( windows[ w ].area.width() - windows[ w ].icon.width()) / 2;
    int y = windows[ w ].area.y() + ( windows[ w ].area.height() - windows[ w ].icon.height()) / 2;
    int width = windows[ w ].icon.width();
    int height = windows[ w ].icon.height();
#ifdef HAVE_OPENGL
    if( dynamic_cast< SceneOpenGL* >( scene ))
        {
//        glPushMatrix();
//        glPushAttrib( GL_ENABLE_BIT );
//        glEnable( GL_BLEND );
//        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
//        windows[ w ].iconTexture.enable();
//        windows[ w ].iconTexture.preparePaint();
//        glBegin( GL_QUADS );
//        glTexCoord2i( 0, 0 );
//        glVertex2i( x, y );
//        glTexCoord2i( windows[ w ].icon.width(), 0 );
//        glVertex2i( x + width, y );
//        glTexCoord2i( windows[ w ].icon.width(), windows[ w ].icon.height());
//        glVertex2i( x + width, y + height );
//        glTexCoord2i( 0, windows[ w ].icon.height());
//        glVertex2i( x, y + height );
//        glEnd();
//        windows[ w ].iconTexture.finishPaint();
//        glPopMatrix();
//        glPopAttrib();
//        windows[ w ].iconTexture.disable();
        }
    else
#endif
        {
#ifdef HAVE_XRENDER
        XRenderComposite( display(),
            windows[ w ].icon.depth() == 32 ? PictOpOver : PictOpSrc,
            windows[ w ].iconPicture, None,
            static_cast< SceneXrender* >( scene )->bufferPicture(),
            0, 0, 0, 0, x, y, width, height );
#endif
        }
    }

} // namespace
