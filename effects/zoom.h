/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

// TODO MIT or some other licence, perhaps move to some lib

#ifndef KWIN_ZOOM_H
#define KWIN_ZOOM_H

#include <effects.h>

namespace KWinInternal
{

class ZoomEffect
    : public Effect
    {
    public:
        ZoomEffect( Workspace* ws );
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
    private:
        double zoom;
        double target_zoom;
        Workspace* wspace;
    };

} // namespace

#endif
