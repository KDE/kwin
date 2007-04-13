/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/


#include "demo_taskbarthumbnail.h"

#include <limits.h>

namespace KWin
{

KWIN_EFFECT( Demo_TaskbarThumbnail, TaskbarThumbnailEffect )

TaskbarThumbnailEffect::TaskbarThumbnailEffect()
    {
    mLastCursorPos = QPoint(-1, -1);
    }


void TaskbarThumbnailEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    // We might need to paint thumbnails if cursor has moved since last
    //  painting or some thumbnails were painted the last time
    QPoint cpos = cursorPos();
    if(cpos != mLastCursorPos || mThumbnails.count() > 0)
        {
        *mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        mThumbnails.clear();
        mLastCursorPos = cpos;
        }

    effects->prePaintScreen(mask, region, time);
    }

void TaskbarThumbnailEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    QRect iconGeo = w->iconGeometry();
    if(iconGeo.contains( mLastCursorPos ))
        mThumbnails.append( w );

    effects->prePaintWindow( w, mask, paint, clip, time );
    }

void TaskbarThumbnailEffect::postPaintScreen()
    {
    // Paint the thumbnails. They need to be painted after other windows
    //  because we want them on top of everything else
    int space = 4;
    foreach( EffectWindow* w, mThumbnails )
        {
        QRect thumb = getThumbnailPosition( w, &space);
        WindowPaintData thumbdata;
        thumbdata.xTranslate = thumb.x() - w->x();
        thumbdata.yTranslate = thumb.y() - w->y();
        thumbdata.xScale = thumb.width() / (float)w->width();
        thumbdata.yScale = thumb.height() / (float)w->height();
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
        float scale = qMin(qMax(icon.height(), 100) / (float)c->height(), 200.0f / c->width());
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
        float scale = qMin(qMax(icon.width(), 75) / (float)c->width(), 200.0f / c->height());
        thumb.setSize( QSize( int(scale * c->width()),int(scale * c->height()) ));
        if( icon.top() < ( displayHeight() - icon.bottom()))  // ...at the top
            thumb.moveTopLeft( QPoint( icon.left(), icon.bottom() + *space ));
        else  // ...at the bottom
            thumb.moveBottomLeft( QPoint( icon.left(), icon.top() - *space ));
        *space += thumb.height() + 8;
        }
    return thumb;
    }

} // namespace

