/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "desktopgrid.h"

#include <kaction.h>
#include <kactioncollection.h>
#include <klocale.h>
#include <qevent.h>

namespace KWin
{

KWIN_EFFECT( DesktopGrid, DesktopGridEffect )

DesktopGridEffect::DesktopGridEffect()
    : progress( 0 )
    , activated( false )
    , keyboard_grab( false )
    {
    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = static_cast< KAction* >( actionCollection->addAction( "ShowDesktopGrid" ));
    a->setText( i18n("Show Desktop Grid" ));
    a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::Key_F8 ));
    connect( a, SIGNAL( triggered( bool )), this, SLOT( toggle()));
    }

void DesktopGridEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( progress != 0 || activated )
        {
        if( activated )
            progress = qMin( 1.0, progress + time / 500. );
        else
            progress = qMax( 0.0, progress - time / 500. );
        // PAINT_SCREEN_BACKGROUND_FIRST is needed because screen will be actually painted more than once,
        // so with normal screen painting second screen paint would erase parts of the first paint
        if( progress != 0 )
            *mask |= PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_BACKGROUND_FIRST;
        if( !activated && progress == 0 )
            finish();
        int d = posToDesktop( cursorPos());
        if( d != hover_desktop )
            {
            *region |= desktopRect( hover_desktop, true );
            hover_desktop = d;
            *region |= desktopRect( hover_desktop, true );
            }
        }
    effects->prePaintScreen( mask, region, time );
    }

void DesktopGridEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    if( progress != 0 )
        {
        if( w->isOnDesktop( painting_desktop ))
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
        else
            w->disablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
        }
    effects->prePaintWindow( w, mask, paint, clip, time );
    }

void DesktopGridEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( progress == 0 )
        {
        effects->paintScreen( mask, region, data );
        return;
        }
    for( int desktop = 1;
         desktop <= effects->numberOfDesktops();
         ++desktop )
        {
        QRect rect = desktopRect( desktop, true );
        if( region.contains( rect )) // this desktop needs painting
            {
            painting_desktop = desktop;
            ScreenPaintData d = data;
            QRect normal = desktopRect( effects->currentDesktop(), false );
            d.xTranslate += rect.x(); // - normal.x();
            d.yTranslate += rect.y(); // - normal.y();
            d.xScale *= rect.width() / float( normal.width());
            d.yScale *= rect.height() / float( normal.height());
            // TODO mask parts that are not visible?
            effects->paintScreen( mask, region, d );
            }
        }
    }

void DesktopGridEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( progress != 0 )
        {
        if( painting_desktop != hover_desktop )
            data.brightness *= 0.7;
        }
    effects->paintWindow( w, mask, region, data );    
    }

void DesktopGridEffect::postPaintScreen()
    {
    if( activated ? progress != 1 : progress != 0 )
        effects->addRepaintFull(); // trigger next animation repaint
    effects->postPaintScreen();
    }

// Gives a position of the given desktop when all desktops are arranged in a grid
QRect DesktopGridEffect::desktopRect( int desktop, bool scaled ) const
    {
    int x, y;
    Qt::Orientation orientation;
    effects->calcDesktopLayout( &x, &y, &orientation );
    --desktop; // make it start with 0
    QRect rect;
    if( orientation == Qt::Vertical )
        rect = QRect(( desktop % x ) * displayWidth(), ( desktop / x ) * displayHeight(),
            displayWidth(), displayHeight());
    else
        rect = QRect(( desktop / y ) * displayWidth(), ( desktop % y ) * displayHeight(),
            displayWidth(), displayHeight());
    if( !scaled )
        return rect;
    QRect current = desktopRect( effects->currentDesktop(), false );
    rect = QRect( int( interpolate( rect.x() - current.x(), rect.x() / float( x ), progress )),
        int( interpolate( rect.y() - current.y(), rect.y() / float( y ), progress )),
        int( interpolate( rect.width(), displayWidth() / float( x ), progress )),
        int( interpolate( rect.height(), displayHeight() / float( y ), progress )));
    return rect;
    }

int DesktopGridEffect::posToDesktop( const QPoint& pos ) const
    {
    for( int desktop = 1; // TODO could be perhaps optimized
         desktop <= effects->numberOfDesktops();
         ++desktop )
        {
        if( desktopRect( desktop, true ).contains( pos ))
            return desktop;
        }
    return 0;
    }

void DesktopGridEffect::desktopChanged( int )
    {
    setActive( false );
    }

void DesktopGridEffect::toggle()
    {
    setActive( !activated );
    }
    
void DesktopGridEffect::setActive( bool active )
    {
    if( activated == active )
        return;
    activated = active;
    if( activated && progress == 0 )
        setup();
    effects->addRepaintFull();
    }

void DesktopGridEffect::setup()
    {
    keyboard_grab = effects->grabKeyboard( this );
    input = effects->createInputWindow( this, 0, 0, displayWidth(), displayHeight(),
        Qt::PointingHandCursor );
    hover_desktop = effects->currentDesktop();
    }

void DesktopGridEffect::finish()
    {
    if( keyboard_grab )
        effects->ungrabKeyboard();
    keyboard_grab = false;
    effects->destroyInputWindow( input );
    effects->addRepaintFull(); // to get rid of hover
    }

void DesktopGridEffect::windowInputMouseEvent( Window, QEvent* e )
    {
    if( e->type() == QEvent::MouseButtonPress )
        {
        effects->setCurrentDesktop( posToDesktop( static_cast< QMouseEvent* >( e )->pos()));
        setActive( false );
        }
    if( e->type() == QEvent::MouseMove )
        {
        int d = posToDesktop( static_cast< QMouseEvent* >( e )->pos());
        if( d != hover_desktop )
            {
            effects->addRepaint( desktopRect( hover_desktop, true ));
            hover_desktop = d;
            effects->addRepaint( desktopRect( hover_desktop, true ));
            }
        }
    }

void DesktopGridEffect::grabbedKeyboardEvent( QKeyEvent* e )
    {
    // TODO
    }

} // namespace

#include "desktopgrid.moc"
