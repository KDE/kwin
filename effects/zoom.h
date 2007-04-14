/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_ZOOM_H
#define KWIN_ZOOM_H

#include <kwineffects.h>

namespace KWin
{

class ZoomEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        ZoomEffect();
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        void cursorMoved( const QPoint& pos, Qt::MouseButtons buttons );
    private slots:
        void zoomIn();
        void zoomOut();
        void actualSize();
    private:
        double zoom;
        double target_zoom;
    };

} // namespace

#endif
