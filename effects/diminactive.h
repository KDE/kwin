/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_DIMINACTIVE_H
#define KWIN_DIMINACTIVE_H

// Include with base class for effects.
#include <kwineffects.h>


namespace KWin
{

class DimInactiveEffect
    : public Effect
    {
    public:
        DimInactiveEffect();
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void windowActivated( EffectWindow* c );
    private:
        EffectWindow* active;
        bool dim_panels; // do/don't dim also all panels
        bool dim_by_group; // keep visible all windows from the active window's group or only the active window
    };

} // namespace

#endif
