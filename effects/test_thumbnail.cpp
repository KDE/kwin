/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/*

 Testing of painting a window more than once. The active window is painted
 once more as a thumbnail in the bottom-right corner of the screen.

*/

#include "test_thumbnail.h"

namespace KWinInternal
{

TestThumbnailEffect::TestThumbnailEffect()
    : active_window( NULL )
    {
    }

void TestThumbnailEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
    if( active_window != NULL && region.contains( thumbnailRect()))
        {
        WindowPaintData data;
        QRect region;
        setPositionTransformations( data, region, active_window, thumbnailRect(), Qt::KeepAspectRatio );
        effects->drawWindow( active_window,
            Scene::PAINT_WINDOW_OPAQUE | Scene::PAINT_WINDOW_TRANSLUCENT | Scene::PAINT_WINDOW_TRANSFORMED,
            region, data );
        }
    }

void TestThumbnailEffect::windowActivated( EffectWindow* act )
    {
    active_window = act;
    workspace()->addRepaint( thumbnailRect());
    }

void TestThumbnailEffect::windowDamaged( EffectWindow* w, const QRect& )
    {
    if( w == active_window )
        workspace()->addRepaint( thumbnailRect());
    // TODO maybe just the relevant part of the area should be repainted?
    }

void TestThumbnailEffect::windowGeometryShapeChanged( EffectWindow* w, const QRect& old )
    {
    if( w == active_window && w->size() != old.size())
        workspace()->addRepaint( thumbnailRect());
    }

void TestThumbnailEffect::windowClosed( EffectWindow* w )
    {
    if( w == active_window )
        {
        active_window = NULL;
        workspace()->addRepaint( thumbnailRect());
        }
    }

QRect TestThumbnailEffect::thumbnailRect() const
    {
    return QRect( displayWidth() - 100, displayHeight() - 100, 100, 100 );
    }

} // namespace
