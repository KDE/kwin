/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "zoom.h"

#include <workspace.h>

namespace KWin
{

ZoomEffect::ZoomEffect()
    : zoom( 1 )
    , target_zoom( 2 )
    {
    }

void ZoomEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( zoom != target_zoom )
        {
        double diff = time / 500.0;
        if( target_zoom > zoom )
            zoom = qMin( zoom + diff, target_zoom );
        else
            zoom = qMax( zoom - diff, target_zoom );
        }
    if( zoom != 1.0 )
        *mask |= Scene::PAINT_SCREEN_TRANSFORMED;
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
        workspace()->addRepaintFull();
    effects->postPaintScreen();
    }

} // namespace
