/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "desktopchangeslide.h"

#include <workspace.h>

namespace KWinInternal
{

const int MAX_PROGRESS = 1000;

DesktopChangeSlideEffect::DesktopChangeSlideEffect()
    : progress( MAX_PROGRESS )
    , painting_old_desktop( false )
    {
    }

void DesktopChangeSlideEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    progress = qBound( 0, progress + time, MAX_PROGRESS );
    // PAINT_SCREEN_BACKGROUND_FIRST is needed because screen will be actually painted twice,
    // so with normal screen painting second screen paint would erase parts of the first paint
    if( progress != MAX_PROGRESS )
        *mask |= Scene::PAINT_SCREEN_TRANSFORMED | Scene::PAINT_SCREEN_BACKGROUND_FIRST;
    effects->prePaintScreen( mask, region, time );
    }

void DesktopChangeSlideEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time )
    {
    if( progress != MAX_PROGRESS )
        {
        if( painting_old_desktop )
            {
            if( w->isOnDesktop( old_desktop ) && !w->isOnCurrentDesktop())
                *mask &= ~Scene::PAINT_WINDOW_DISABLED; // paint windows on old desktop
            else
                *mask |= Scene::PAINT_WINDOW_DISABLED;
            }
        else
            {
            if( w->isOnAllDesktops())
                *mask |= Scene::PAINT_WINDOW_TRANSFORMED;
            }
        }
    effects->prePaintWindow( w, mask, region, time );
    }

void DesktopChangeSlideEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( progress == MAX_PROGRESS )
        {
        effects->paintScreen( mask, region, data );
        return;
        }
    // old desktop
    painting_old_desktop = true;
    ScreenPaintData d1 = data;
    d1.yTranslate += displayHeight() * progress / 1000;
    effects->paintScreen( mask, region, d1 );
    painting_old_desktop = false;
    // new (current) desktop
    ScreenPaintData d2 = data;
    d2.yTranslate -= displayHeight() * ( MAX_PROGRESS - progress ) / 1000;
    effects->paintScreen( mask, region, d2 );
    }

void DesktopChangeSlideEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( progress != MAX_PROGRESS )
        { // don't move windows on all desktops (compensate screen transformation)
        if( w->isOnAllDesktops())
            data.yTranslate += displayHeight() * ( MAX_PROGRESS - progress ) / 1000;
        }
    effects->paintWindow( w, mask, region, data );
    }

void DesktopChangeSlideEffect::postPaintScreen()
    {
    if( progress != MAX_PROGRESS )
        Workspace::self()->addDamageFull(); // trigger next animation repaint
    effects->postPaintScreen();
    }

void DesktopChangeSlideEffect::desktopChanged( int old )
    {
    if( old != old_desktop )
        {
        old_desktop = old;
        progress = 0;
        Workspace::self()->addDamageFull();
        }
    }

} // namespace
