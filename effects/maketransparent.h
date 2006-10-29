/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

// TODO MIT or some other licence, perhaps move to some lib

#ifndef KWIN_MAKETRANSPARENT_H
#define KWIN_MAKETRANSPARENT_H

#include <effects.h>

namespace KWinInternal
{

class MakeTransparentEffect
    : public Effect
    {
    public:
        virtual void windowUserMovedResized( Toplevel* c, bool first, bool last );
        virtual void prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time );
        virtual void paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data );
    };

} // namespace

#endif
