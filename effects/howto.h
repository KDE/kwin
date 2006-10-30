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

*/

#ifndef KWIN_HOWTO_H
#define KWIN_HOWTO_H

// Include with base class for effects.
#include <effects.h>

// Everything in KWin is in a namespace. There's no need to prefix names
// with KWin or K, there's no (big) need to care about symbol clashes.
namespace KWinInternal
{

// The class implementing the effect.
class HowtoEffect
// Inherit from the base class for effects.
    : public Effect
    {
    public:
        // There are two kinds of functions in an effect:
        
        // Functions related to painting: These allow the effect to affect painting.

        // A pre-paint function. It tells the compositing code how the painting will
        // be affected by this effect.        
        virtual void prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time );
        
        // A paint function. It actually performs the modifications to the painting.
        virtual void paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data );
        
        // A post-paint function. It can be used for cleanups after painting, but with animations
        // it is also used to trigger repaints during the next painting pass by manually "damaging"
        // areas of the window.
        virtual void postPaintWindow( Scene::Window* w );
        
        // Notification functions: These inform the effect about changes such as a new window
        // being activated.

        // The given window has been deleted (destroyed).
        virtual void windowDeleted( Toplevel* c );
        
        // The given window has been activated.
        virtual void windowActivated( Toplevel* c );
    private:
        // The window that will be faded out and in again.
        Toplevel* fade_window;
        
        // The progress of the fading.
        int progress;
    };

} // namespace

#endif
