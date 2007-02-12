/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "fade.h"

#include <client.h>
#include <deleted.h>

namespace KWinInternal
{

FadeEffect::FadeEffect()
    : fade_in_speed( 20 )
    , fade_out_speed( 70 )
    {
    }

void FadeEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time )
    {
    if( windows.contains( w ))
        {
        if( windows[ w ].current < windows[ w ].target )
            {
            windows[ w ].current += time / double( 10000 / fade_in_speed ) * windows[ w ].step_mult;
            if( windows[ w ].current > windows[ w ].target )
                windows[ w ].current = windows[ w ].target;
            }
        else if( windows[ w ].current > windows[ w ].target )
            {
            windows[ w ].current -= time / double( 10000 / fade_out_speed ) * windows[ w ].step_mult;
            if( windows[ w ].current < windows[ w ].target )
                windows[ w ].current = windows[ w ].target;
            }

        if( !windows[ w ].isFading())
            {
            if( windows[ w ].deleted )
                static_cast< Deleted* >( w->window())->unrefWindow();
            }
        else
            {
            *mask |= Scene::PAINT_WINDOW_TRANSLUCENT;
            *mask &= ~Scene::PAINT_WINDOW_OPAQUE;
            if( windows[ w ].deleted )
                w->enablePainting( Scene::Window::PAINT_DISABLED_BY_DELETE );
            }
        }
    effects->prePaintWindow( w, mask, region, time );
    }

void FadeEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( windows.contains( w ))
        data.opacity = ( data.opacity + ( windows[ w ].opacity_is_zero ? 1 : 0 )) * windows[ w ].current;
    effects->paintWindow( w, mask, region, data );
    }

void FadeEffect::postPaintWindow( EffectWindow* w )
    {
    if( windows.contains( w ) && windows.value( w ).isFading())
        w->window()->addRepaintFull(); // trigger next animation repaint
    effects->postPaintWindow( w );
    }

void FadeEffect::windowOpacityChanged( EffectWindow* c )
    {
    double new_opacity = c->window()->opacity();
    if( !windows.contains( c ))
        { // we don't know the old opacity, so don't fade
        windows[ c ].current = 1;
        windows[ c ].target = 1;
        }
    else
        {
        if( windows[ c ].old_opacity == 0.0 )
            windows[ c ].old_opacity = 1;
        if( new_opacity == 0.0 )
            { // special case; if opacity is 0, we can't just multiply data.opacity
            windows[ c ].opacity_is_zero = true;
            windows[ c ].current = windows[ c ].current * windows[ c ].old_opacity;
            windows[ c ].target = 0;
            windows[ c ].step_mult = 1;
            }
        else
            {
            windows[ c ].opacity_is_zero = false;
            windows[ c ].current = ( windows[ c ].current * windows[ c ].old_opacity ) / new_opacity;
            windows[ c ].target = 1;
            windows[ c ].step_mult = 1 / new_opacity;
            }
        }
    windows[ c ].old_opacity = new_opacity;
    c->window()->addRepaintFull();
    }

void FadeEffect::windowAdded( EffectWindow* c )
    {
    if( !windows.contains( c ))
        {
        windows[ c ].old_opacity = c->window()->opacity();
        windows[ c ].current = 0;
        }
    if( c->window()->opacity() == 0.0 )
        {
        windows[ c ].opacity_is_zero = true;
        windows[ c ].target = 0;
        windows[ c ].step_mult = 1;
        }
    else
        {
        windows[ c ].opacity_is_zero = false;
        windows[ c ].target = 1;
        windows[ c ].step_mult = 1 / c->window()->opacity();
        }
    c->window()->addRepaintFull();
    }

void FadeEffect::windowClosed( EffectWindow* c )
    {
    if( !windows.contains( c ))
        {
        windows[ c ].old_opacity = c->window()->opacity();
        windows[ c ].current = 1;
        }
    if( c->window()->opacity() == 0.0 )
        {
        windows[ c ].opacity_is_zero = true;
        windows[ c ].step_mult = 1;
        }
    else
        {
        windows[ c ].opacity_is_zero = false;
        windows[ c ].step_mult = 1 / c->window()->opacity();
        }
    windows[ c ].deleted = true;
    windows[ c ].target = 0;
    c->window()->addRepaintFull();
    static_cast< Deleted* >( c->window())->refWindow();
    }

void FadeEffect::windowDeleted( EffectWindow* c )
    {
    windows.remove( c );
    }

} // namespace
