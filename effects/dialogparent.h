/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_DIALOGPARENT_H
#define KWIN_DIALOGPARENT_H

// Include with base class for effects.
#include <effects.h>


namespace KWin
{

/**
 * An effect which changes saturation and brighness of windows which have
 *  active modal dialogs.
 * This should make the dialog seem more important and emphasize that the
 *  window is inactive until the dialog is closed.
 **/
class DialogParentEffect
    : public Effect
    {
    public:
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w );

        virtual void windowClosed( EffectWindow* c );
        virtual void windowActivated( EffectWindow* c );

    protected:
        bool hasModalWindow( Toplevel* t );
    private:
        // The progress of the fading.
        QHash<EffectWindow*, float> effectStrength;
    };

} // namespace

#endif
