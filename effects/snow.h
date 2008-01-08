/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2007 Martin Gräßlin <ubuntu@martin-graesslin.com

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/


#ifndef KWIN_SNOW_H
#define KWIN_SNOW_H

#include <kwineffects.h>
#include <kwinglutils.h>
#include <qobject.h>

#include <QList>
#include <QTime>

namespace KWin
{

class SnowEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        SnowEffect();
        virtual ~SnowEffect();
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();

    private slots:
        void toggle();
    private:
        void loadTexture();
        GLTexture* texture;
        QList<QRect>* flakes;
        QTime lastFlakeTime;
        long nextFlakeMillis;
        int mNumberFlakes;
        int mMinFlakeSize;
        int mMaxFlakeSize;
        bool active;
    };

} // namespace

#endif
