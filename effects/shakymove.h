/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

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
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void windowUserMovedResized( EffectWindow* c, bool first, bool last );
        virtual void windowClosed( EffectWindow* c );
    private slots:
        void tick();
    private:
        QHash< const EffectWindow*, int > windows;
        QTimer timer;
    };

} // namespace

#endif
