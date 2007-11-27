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

#include "demo_wavywindows.h"

#include <math.h>


namespace KWin
{

KWIN_EFFECT( demo_wavywindows, WavyWindowsEffect )

WavyWindowsEffect::WavyWindowsEffect()
    {
    mTimeElapsed = 0.0f;
    }


void WavyWindowsEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    // Adjust elapsed time
    mTimeElapsed += (time / 1000.0f);
    // We need to mark the screen windows as transformed. Otherwise the whole
    //  screen won't be repainted, resulting in artefacts
    data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, time);
    }

void WavyWindowsEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    // This window will be transformed by the effect
    data.setTransformed();
    // Check if OpenGL compositing is used
    // Request the window to be divided into cells which are at most 30x30
    //  pixels big
    data.quads = data.quads.makeGrid( 30 );

    effects->prePaintWindow( w, data, time );
    }

void WavyWindowsEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    // Make sure we have OpenGL compositing and the window is vidible and not a
    //  special window
// TODO    if( w->isVisible() && !w->isSpecialWindow() )
    if( !w->isSpecialWindow() )
        {
        // We have OpenGL compositing and the window has been subdivided
        //  because of our request (in pre-paint pass)
        // Transform all the vertices to create wavy effect
        for( int i = 0;
             i < data.quads.count();
             ++i )
            for( int j = 0;
                 j < 4;
                 ++j )
                {
                WindowVertex& v = data.quads[ i ][ j ];
                v.move( v.x() + sin(mTimeElapsed + v.originalY() / 60 + 0.5f) * 10,
                    v.y() + sin(mTimeElapsed + v.originalX() / 80) * 10 );
            }
        }

    // Call the next effect.
    effects->paintWindow( w, mask, region, data );
    }

void WavyWindowsEffect::postPaintScreen()
    {
    // Repaint the workspace so that everything would be repainted next time
    effects->addRepaintFull();

    // Call the next effect.
    effects->postPaintScreen();
    }

} // namespace

