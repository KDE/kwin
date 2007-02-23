/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_SHIFTWORKSPACEUP_H
#define KWIN_SHIFTWORKSPACEUP_H

#include <qtimer.h>

#include <effects.h>

namespace KWinInternal
{

class ShiftWorkspaceUpEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        ShiftWorkspaceUpEffect();
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
    private slots:
        void tick();
    private:
        QTimer timer;
        bool up;
        int diff;
    };

} // namespace

#endif
