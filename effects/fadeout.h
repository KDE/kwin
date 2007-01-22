/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

// TODO MIT or some other licence, perhaps move to some lib

#ifndef KWIN_FADEOUT_H
#define KWIN_FADEOUT_H

#include <effects.h>

namespace KWinInternal
{

class FadeOutEffect
    : public Effect
    {
    public:
        virtual void prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time );
        virtual void paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( Scene::Window* w );
        // TODO react also on virtual desktop changes
        virtual void windowClosed( Toplevel* c, Deleted* d );
        virtual void windowDeleted( Deleted* c );
    private:
        QMap< const Toplevel*, double > windows;
    };

} // namespace

#endif
