/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_SCALEIN_H
#define KWIN_SCALEIN_H

#include <effects.h>

namespace KWinInternal
{

class ScaleInEffect
    : public Effect
    {
    public:
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w );
        // TODO react also on virtual desktop changes
        virtual void windowAdded( EffectWindow* c );
        virtual void windowClosed( EffectWindow* c );
    private:
        QMap< const EffectWindow*, double > windows;
    };

} // namespace

#endif
