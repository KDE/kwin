/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "zoom.h"

#include <kaction.h>
#include <kactioncollection.h>
#include <kstandardaction.h>

namespace KWin
{

KWIN_EFFECT( Zoom, ZoomEffect )

ZoomEffect::ZoomEffect()
    : zoom( 1 )
    , target_zoom( 1 )
    {
    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a;
    a = static_cast< KAction* >( actionCollection->addAction( KStandardAction::ZoomIn, this, SLOT( zoomIn())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Equal));
    a = static_cast< KAction* >( actionCollection->addAction( KStandardAction::ZoomOut, this, SLOT( zoomOut())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Minus));
    a = static_cast< KAction* >( actionCollection->addAction( KStandardAction::ActualSize, this, SLOT( actualSize())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_0));
    }

void ZoomEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( zoom != target_zoom )
        {
        double diff = time / 500.0;
        if( target_zoom > zoom )
            zoom = qMin( zoom * qMax( 1 + diff, 1.2 ), target_zoom );
        else
            zoom = qMax( zoom * qMin( 1 - diff, 0.8 ), target_zoom );
        }
    if( zoom != 1.0 )
        *mask |= PAINT_SCREEN_TRANSFORMED;
    effects->prePaintScreen( mask, region, time );
    }

void ZoomEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( zoom != 1.0 )
        {
        data.xScale *= zoom;
        data.yScale *= zoom;
        QPoint cursor = cursorPos();
        // set the position so that the cursor is in the same position in the scaled view
        data.xTranslate = - int( cursor.x() * ( zoom - 1 ));
        data.yTranslate = - int( cursor.y() * ( zoom - 1 ));
        }
    effects->paintScreen( mask, region, data );
    }

void ZoomEffect::postPaintScreen()
    {
    if( zoom != target_zoom )
        effects->addRepaintFull();
    effects->postPaintScreen();
    }

void ZoomEffect::zoomIn()
    {
    target_zoom *= 1.2;
    effects->addRepaintFull();
    }

void ZoomEffect::zoomOut()
    {
    target_zoom /= 1.2;
    if( target_zoom < 1 )
        target_zoom = 1;
    effects->addRepaintFull();
    }

void ZoomEffect::actualSize()
    {
    target_zoom = 1;
    effects->addRepaintFull();
    }

} // namespace

#include "zoom.moc"
