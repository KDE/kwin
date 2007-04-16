/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_TRACKMOUSE_H
#define KWIN_TRACKMOUSE_H

#include <kwineffects.h>
#include <kwinglutils.h>

namespace KWin
{

class TrackMouseEffect
    : public Effect
    {
    public:
        TrackMouseEffect();
        virtual ~TrackMouseEffect();
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void cursorMoved( const QPoint& pos, Qt::MouseButtons buttons );
    private:
        QRect starRect( int num ) const;
        void loadTexture();
        bool active;
        int angle;
        GLTexture* texture;
        QSize textureSize;
    };

} // namespace

#endif
