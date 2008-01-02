/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#include "zoom.h"

#include <kaction.h>
#include <kactioncollection.h>
#include <kstandardaction.h>

namespace KWin
{

KWIN_EFFECT( zoom, ZoomEffect )

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

void ZoomEffect::prePaintScreen( ScreenPrePaintData& data, int time )
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
        data.mask |= PAINT_SCREEN_TRANSFORMED;
    effects->prePaintScreen( data, time );
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

void ZoomEffect::mouseChanged( const QPoint& pos, const QPoint& old, Qt::MouseButtons,
    Qt::MouseButtons, Qt::KeyboardModifiers, Qt::KeyboardModifiers )
    {
    if( pos != old && zoom != 1 )
        effects->addRepaintFull();
    }

} // namespace

#include "zoom.moc"
