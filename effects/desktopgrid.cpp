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

namespace KWin
{

KWIN_EFFECT( DesktopGrid, DesktopGridEffect )

DesktopGridEffect::DesktopGridEffect()
    : progress( 0 )
    , activated( false )
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

void DesktopGridEffect::postPaintScreen()
    {
    if( activated ? progress != 1 : progress != 0 )
        effects->addRepaintFull(); // trigger next animation repaint
    effects->postPaintScreen();
    }

// Gives a position of the given desktop when all desktops are arranged in a grid
QRect DesktopGridEffect::desktopRect( int desktop, bool scaled )
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

void DesktopGridEffect::desktopChanged( int )
    {
    if( activated )
        toggle();
    }

void DesktopGridEffect::toggle()
    {
    activated = !activated;
    effects->addRepaintFull();
    }

} // namespace

#include "desktopgrid.moc"
