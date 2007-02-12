/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/*

 Files howto.cpp and howto.h implement HowtoEffect, a commented demo compositing
 effect that fades out and again in a window after it has been activated.

 Please see file howto.h first.
 
*/

// Include the class definition.
#include "howto.h"

#include "deleted.h"

// Note that currently effects need to be manually enabled in the EffectsHandler
// class constructor (in effects.cpp).

namespace KWinInternal
{

// A pre-paint function that tells the compositing code how this effect will affect
// the painting. During every painting pass this function is called first, before
// the actual painting function.
// Arguments:
// w - the window that will be painted
// mask - a mask of flags controlling the painting, setting or resetting flags changes
//   how the painting will be done
// region - the region of the screen that needs to be painted, support for modifying it
//   is not fully implemented yet, do not use
// time - time in milliseconds since the last paint, useful for animations
void HowtoEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time )
    {
    // Is this window the one that is going to be faded out and in again?
    if( w == fade_window )
        {
        // Simply add the time to the total progress. The value of progress will be used
        // to determine how far in effect is.
        progress += time;
        // If progress is < 1000 (milliseconds), the effect is still in progress.
        if( progress < 1000 ) // complete change in 1000ms
            {
            // Since the effect will make the window translucent, explicitly change
            // the flags so that the window will be painted only as translucent.
            *mask |= Scene::PAINT_WINDOW_TRANSLUCENT;
            *mask &= ~Scene::PAINT_WINDOW_OPAQUE;
            }
        else
            {
            // If progress has reached 1000 (milliseconds), it means the effect is done
            // and there is no window to fade anymore.
            fade_window = NULL;
            }
        }
    // Call the next effect (or the actual window painting code if this is the last effect).
    // Effects are chained and they all modify something if needed and then call the next one.
    effects->prePaintWindow( w, mask, region, time );
    }

// The function that handles the actual painting. Some simple modifications are possible
// by only changing the painting data. More complicated effects would do some painting
// or transformations directly.
// Arguments:
// w - the window that will be painted
// mask - a mask of flags controlling the painting
// region - the region of the screen that needs to be painted, if mask includes the TRANSFORMED
//   then special care needs to be taken, because the region may be infiniteRegion(), meaning
//   everything needs to be painted
// data - painting data that can be modified to do some simple transformations
void HowtoEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    // Is this the window to be faded out and in again?
    if( w == fade_window )
        {
        // This effect, after a window has been activated, fades it out to only 50% transparency
        // and then fades it in again to be fully opaque (assuming it's otherwise a fully opaque
        // window, otherwise the transparencies will be added).
        if( progress <= 500 )
            {
            // For the first 500 milliseconds (progress being 0 to 500), the window is faded out.
            // progress == 0   -> opacity *= 1
            // progress == 500 -> opacity *= 0.5
            // Note that the division is floating-point division to avoid integer rounding down.
            // Note that data.opacity is not set but multiplied - this allows chaining of effects,
            // for example if another effect always changes opacity of some window types.
            data.opacity *= 1 - 0.5 * ( progress / 500.0 );
            }
        else
            {
            // For the second 500 milliseconds (progress being 500 to 1000), the window is
            // faded in again.
            // progress == 500   -> opacity *= 0.5
            // progress == 1000  -> opacity *= 1
            data.opacity *= 0.5 + 0.5 * ( progress - 500 ) / 500.0;
            }
        }
    // Call the next effect.
    effects->paintWindow( w, mask, region, data );
    }

// The function that is called after the painting pass is finished. When an animation is going on,
// it can add repaints of some areas so that the next painting pass has to repaint them again.
void HowtoEffect::postPaintWindow( EffectWindow* w )
    {
    // Is this the window to be faded out and in again?
    if( w == fade_window )
        {
        // Trigger repaint of the whole window, this will cause it to be repainted the next painting pass.
        // Currently the API for effects is not complete, so for now window() is used to access
        // internal class Toplevel. This should change in the future.
        w->window()->addRepaintFull(); // trigger next animation repaint
        }
    // Call the next effect.
    effects->postPaintWindow( w );
    }

// This function is called when a new window becomes active.
void HowtoEffect::windowActivated( EffectWindow* c )
    {
    // Set the window to be faded (or NULL if no window is active).
    fade_window = c;
    if( fade_window != NULL )
        {
        // If there is a window to be faded, reset the progress to zero.
        progress = 0;
        // And add repaint to the window so that it needs to be repainted.
        c->window()->addRepaintFull();
        }
    }

// This function is called when a window is closed.
void HowtoEffect::windowClosed( EffectWindow* c )
    {
    // If the window to be faded out and in is closed, just reset the pointer.
    // This effect then will do nothing and just call the next effect.
    if( fade_window == c )
        fade_window = NULL;
    }

// That's all. Don't forget to enable the effect as described at the beginning of this file.

} // namespace
