/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_DRUNKEN_H
#define KWIN_DRUNKEN_H

#include <effects.h>

namespace KWinInternal
{

class DrunkenEffect
    : public Effect
    {
    public:
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w );
        virtual void windowAdded( EffectWindow* w );
        virtual void windowClosed( EffectWindow* w );
    private:
        QHash< EffectWindow*, double > windows; // progress
    };

} // namespace

#endif
