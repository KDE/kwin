/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_DESKTOPGRID_H
#define KWIN_DESKTOPGRID_H

#include <kwineffects.h>
#include <qobject.h>

namespace KWin
{

class DesktopGridEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        DesktopGridEffect();
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time );
        virtual void desktopChanged( int old );
    private slots:
        void toggle();
    private:
        QRect desktopRect( int desktop, bool scaled );
        float progress;
        bool activated;
        int painting_desktop;
    };

} // namespace

#endif
