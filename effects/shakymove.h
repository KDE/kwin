/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

// TODO MIT or some other licence, perhaps move to some lib

#ifndef KWIN_SHAKYMOVE_H
#define KWIN_SHAKYMOVE_H

#include <qtimer.h>

#include <effects.h>

namespace KWinInternal
{

class ShakyMoveEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        ShakyMoveEffect();
        virtual void windowUserMovedResized( Toplevel* c, bool first, bool last );
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time );
        virtual void paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data );
        virtual void windowDeleted( Toplevel* c );
    private slots:
        void tick();
    private:
        QMap< const Toplevel*, int > windows;
        QTimer timer;
    };

} // namespace

#endif
