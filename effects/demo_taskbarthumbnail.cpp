/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#include "demo_taskbarthumbnail.h"

#include <limits.h>

namespace KWin
{

KWIN_EFFECT( demo_taskbarthumbnail, TaskbarThumbnailEffect )

TaskbarThumbnailEffect::TaskbarThumbnailEffect()
    {
    mLastCursorPos = QPoint(-1, -1);
    }


void TaskbarThumbnailEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    // We might need to paint thumbnails if cursor has moved since last
    //  painting or some thumbnails were painted the last time
    QPoint cpos = cursorPos();
    if(cpos != mLastCursorPos || mThumbnails.count() > 0)
        {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        mThumbnails.clear();
        mLastCursorPos = cpos;
        }

    effects->prePaintScreen(data, time);
    }

void TaskbarThumbnailEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    QRect iconGeo = w->iconGeometry();
    if(iconGeo.contains( mLastCursorPos ))
        mThumbnails.append( w );

    effects->prePaintWindow( w, data, time );
    }

void TaskbarThumbnailEffect::postPaintScreen()
    {
    // Paint the thumbnails. They need to be painted after other windows
    //  because we want them on top of everything else
    int space = 4;
    foreach( EffectWindow* w, mThumbnails )
        {
        QRect thumb = getThumbnailPosition( w, &space);
        WindowPaintData thumbdata( w );
        thumbdata.xTranslate = thumb.x() - w->x();
        thumbdata.yTranslate = thumb.y() - w->y();
        thumbdata.xScale = thumb.width() / (double)w->width();
        thumbdata.yScale = thumb.height() / (double)w->height();
        // From Scene::Window::infiniteRegion()
        QRegion infRegion = QRegion( INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX );
        effects->paintWindow( w, PAINT_WINDOW_TRANSFORMED, infRegion, thumbdata );
        }

    // Call the next effect.
    effects->postPaintScreen();
    }

QRect TaskbarThumbnailEffect::getThumbnailPosition( EffectWindow* c, int* space ) const
    {
    QRect thumb;
    QRect icon = c->iconGeometry();

    // Try to figure out if taskbar is horizontal or vertical
    if( icon.right() < 40 || ( displayWidth() - icon.left()) < 40 )
        {
        // Vertical taskbar...
        double scale = qMin(qMax(icon.height(), 100) / (double)c->height(), 200.0 / c->width());
        thumb.setSize( QSize( int(scale * c->width()),int(scale * c->height()) ));
        if( icon.right() < 40 )  // ...on the left
            thumb.moveTopLeft( QPoint( icon.right() + *space, icon.top() ));
        else  // ...on the right
            thumb.moveTopRight( QPoint( icon.left() - *space, icon.top()));
        *space += thumb.width() + 8;
        }
    else
        {
        // Horizontal taskbar...
        double scale = qMin(qMax(icon.width(), 75) / (double)c->width(), 200.0 / c->height());
        thumb.setSize( QSize( int(scale * c->width()),int(scale * c->height()) ));
        if( icon.top() < ( displayHeight() - icon.bottom()))  // ...at the top
            thumb.moveTopLeft( QPoint( icon.left(), icon.bottom() + *space ));
        else  // ...at the bottom
            thumb.moveBottomLeft( QPoint( icon.left(), icon.top() - *space ));
        *space += thumb.height() + 8;
        }
    return thumb;
    }

void TaskbarThumbnailEffect::mouseChanged( const QPoint& pos, const QPoint&,
    Qt::MouseButtons, Qt::MouseButtons, Qt::KeyboardModifiers, Qt::KeyboardModifiers )
    {
    // this should check if the mouse position change actually means something
    // (just like it should be done in prePaintScreen()), but since this effect
    // will be replaced in the future, just trigger a repaint
    if( pos != mLastCursorPos )
        effects->addRepaintFull();
    }

} // namespace
