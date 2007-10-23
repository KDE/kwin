/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_SHOWPAINT_H
#define KWIN_SHOWPAINT_H

#include <kwineffects.h>

namespace KWin
{

class ShowPaintEffect
    : public Effect
    {
    public:
        ShowPaintEffect();
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
    private:
        void paintGL();
        void paintXrender();
        QRegion painted; // what's painted in one pass
        int color_index;
    };

} // namespace

#endif
