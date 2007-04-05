/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "boxswitch.h"

#include "tabbox.h"
#include "client.h"
#include "scene_xrender.h"
#include "scene_opengl.h"

#include <QSize>

#ifdef HAVE_OPENGL
#include <GL/gl.h>
#endif

namespace KWin
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

void BoxSwitchEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
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
    effects->prePaintWindow( w, mask, paint, clip, time );
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
                    paintHighlight( windows[ w ]->area,
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

                foreach( painting_desktop, desktops.keys())
                    {
                    if( painting_desktop == selected_desktop )
                        {
                        paintHighlight( desktops[ painting_desktop ]->area,
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
    if( mMode == TabBox::WindowsMode )
        {
        foreach( EffectWindow* w, windows.keys())
            {
            if( windows[ w ]->clickable.contains( pos ))
                {
                Workspace::self()->setTabBoxClient( static_cast< Client* >( w->window()));
                }
            }
        }
    else
        {
        foreach( int i, desktops.keys())
            {
            if( desktops[ i ]->clickable.contains( pos ))
                {
                Workspace::self()->setTabBoxDesktop( i );
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
                workspace()->addRepaint( windows[ w ]->area );
                }
            }
        else
            {
            if( w->isOnAllDesktops())
                {
                foreach( ItemInfo* info, desktops )
                    workspace()->addRepaint( info->area );
                }
            else
                {
                workspace()->addRepaint( desktops[ w->desktop() ]->area );
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
                workspace()->addRepaint( windows[ w ]->area );
                }
            }
        else
            {
            if( w->isOnAllDesktops())
                {
                foreach( ItemInfo* info, desktops )
                    workspace()->addRepaint( info->area );
                }
            else
                {
                workspace()->addRepaint( desktops[ w->desktop() ]->area );
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
            if( Workspace::self()->currentTabBoxClientList().count() > 0 )
                {
                mMode = mode;
                Workspace::self()->refTabBox();
                setActive();
                }
            }
        else
            { // DesktopMode
            if( Workspace::self()->currentTabBoxDesktopList().count() > 0 )
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
                    workspace()->addRepaint( windows.value( selected_window )->area );
                selected_window->window()->addRepaintFull();
                }
            selected_window = Workspace::self()->currentTabBoxClient()->effectWindow();
            if( windows.contains( selected_window ))
                workspace()->addRepaint( windows.value( selected_window )->area );
            selected_window->window()->addRepaintFull();
            if( Workspace::self()->currentTabBoxClientList() == original_windows )
                return;
            original_windows = Workspace::self()->currentTabBoxClientList();
            }
        else
            { // DesktopMode
            if( desktops.contains( selected_desktop ))
                workspace()->addRepaint( desktops.value( selected_desktop )->area );
            selected_desktop = Workspace::self()->currentTabBoxDesktop();
            if( desktops.contains( selected_desktop ))
                workspace()->addRepaint( desktops.value( selected_desktop )->area );
            if( Workspace::self()->currentTabBoxDesktopList() == original_desktops )
                return;
            original_desktops = Workspace::self()->currentTabBoxDesktopList();
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
        original_windows = Workspace::self()->currentTabBoxClientList();
        selected_window = Workspace::self()->currentTabBoxClient()->effectWindow();
        }
    else
        {
        original_desktops = Workspace::self()->currentTabBoxDesktopList();
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
            }
        }
    }

void BoxSwitchEffect::setInactive()
    {
    mActivated = false;
    Workspace::self()->unrefTabBox();
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
            }
        foreach( ItemInfo* i, windows )
            {
#ifdef HAVE_XRENDER
            if( dynamic_cast< SceneXrender* >( scene ))
                {
                if( i->iconPicture != None )
                    XRenderFreePicture( display(), i->iconPicture );
                i->iconPicture = None;
                }
#endif
            delete i;
            }
        windows.clear();
        }
    else
        { // DesktopMode
        foreach( ItemInfo* i, desktops )
            delete i;
        desktops.clear();
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
        for( int i = 0; i < original_windows.count(); i++ )
            {
            EffectWindow* w = original_windows.at( i )->effectWindow();
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
    double alpha = 0.75;
#ifdef HAVE_OPENGL
    if( dynamic_cast< SceneOpenGL* >( scene ))
        {
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        glColor4f( 0, 0, 0, alpha );
        glEnableClientState( GL_VERTEX_ARRAY );
        int verts[ 4 * 2 ] =
            {
            frame_area.x(), frame_area.y(),
            frame_area.x(), frame_area.y() + frame_area.height(),
            frame_area.x() + frame_area.width(), frame_area.y() + frame_area.height(),
            frame_area.x() + frame_area.width(), frame_area.y()
            };
        glVertexPointer( 2, GL_INT, 0, verts );
        glDrawArrays( GL_QUADS, 0, 4 );
        glDisableClientState( GL_VERTEX_ARRAY );
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
        glEnableClientState( GL_VERTEX_ARRAY );
        int verts[ 4 * 2 ] =
            {
            area.x(), area.y(),
            area.x(), area.y() + area.height(),
            area.x() + area.width(), area.y() + area.height(),
            area.x() + area.width(), area.y()
            };
        glVertexPointer( 2, GL_INT, 0, verts );
        glDrawArrays( GL_QUADS, 0, 4 );
        glDisableClientState( GL_VERTEX_ARRAY );
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

void BoxSwitchEffect::paintWindowThumbnail( EffectWindow* w )
    {
    if( !windows.contains( w ))
        return;
    WindowPaintData data;

    setPositionTransformations( data,
        windows[ w ]->thumbnail, w,
        windows[ w ]->area.adjusted( highlight_margin, highlight_margin, -highlight_margin, -highlight_margin ),
        Qt::KeepAspectRatio );

    effects->drawWindow( w,
        Scene::PAINT_WINDOW_OPAQUE | Scene::PAINT_WINDOW_TRANSFORMED,
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

    effects->paintScreen( Scene::PAINT_SCREEN_TRANSFORMED | Scene::PAINT_SCREEN_BACKGROUND_FIRST,
        region, data );
    }

void BoxSwitchEffect::paintWindowIcon( EffectWindow* w )
    {
    if( !windows.contains( w ))
        return;
    if( windows[ w ]->icon.serialNumber()
        != static_cast< Client* >( w->window())->icon().serialNumber())
        { // make sure windows[ w ]->icon is the right QPixmap, and rebind
        windows[ w ]->icon = static_cast< Client* >( w->window())->icon();
#ifdef HAVE_OPENGL
        if( dynamic_cast< SceneOpenGL* >( scene ))
            {
            windows[ w ]->iconTexture.load( windows[ w ]->icon );
            windows[ w ]->iconTexture.setFilter( GL_LINEAR );
            }
        else
#endif
            {
#ifdef HAVE_XRENDER
            if( windows[ w ]->iconPicture != None )
                XRenderFreePicture( display(), windows[ w ]->iconPicture );
            windows[ w ]->iconPicture = XRenderCreatePicture( display(),
                windows[ w ]->icon.handle(), alphaFormat, 0, NULL );
#endif
            }
        }
    int width = windows[ w ]->icon.width();
    int height = windows[ w ]->icon.height();
    int x = windows[ w ]->area.x() + windows[ w ]->area.width() - width - highlight_margin;
    int y = windows[ w ]->area.y() + windows[ w ]->area.height() - height - highlight_margin;
#ifdef HAVE_OPENGL
    if( dynamic_cast< SceneOpenGL* >( scene ))
        {
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        windows[ w ]->iconTexture.bind();
        glEnableClientState( GL_VERTEX_ARRAY );
        int verts[ 4 * 2 ] =
            {
            x, y,
            x, y + height,
            x + width, y + height,
            x + width, y
            };
        glVertexPointer( 2, GL_INT, 0, verts );
        glEnableClientState( GL_TEXTURE_COORD_ARRAY );
        int texcoords[ 4 * 2 ] =
            {
            0, 1,
            0, 0,
            1, 0,
            1, 1
            };
        glTexCoordPointer( 2, GL_INT, 0, texcoords );
        glDrawArrays( GL_QUADS, 0, 4 );
        glDisableClientState( GL_TEXTURE_COORD_ARRAY );
        glDisableClientState( GL_VERTEX_ARRAY );
        windows[ w ]->iconTexture.unbind();
        glPopAttrib();
        }
    else
#endif
        {
#ifdef HAVE_XRENDER
        XRenderComposite( display(),
            windows[ w ]->icon.depth() == 32 ? PictOpOver : PictOpSrc,
            windows[ w ]->iconPicture, None,
            static_cast< SceneXrender* >( scene )->bufferPicture(),
            0, 0, 0, 0, x, y, width, height );
#endif
        }
    }

} // namespace
