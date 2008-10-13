/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2007 Martin Gräßlin <ubuntu@martin-graesslin.com>
 Copyright (C) 2008 Torgny Johansson <torgny.johansson@gmail.com>

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

class SnowFlake
    {
    public:
        SnowFlake(int x, int y, int width, int height, int maxVSpeed, int maxHSpeed);
        virtual ~SnowFlake();
        int getHSpeed();
        int getVSpeed();
        float getRotationAngle();
        void updateSpeedAndRotation();
        int getHeight();
        int getWidth();
        int getX();
        int getY();
        QRect getRect();
        void setHeight(int height);
        void setWidth(int width);
        void setX(int x);
        void setY(int y);

    private:
        QRect rect;
        int vSpeed;
        int hSpeed;
        float rotationAngle;
        float rotationSpeed;
    };

class SnowEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        SnowEffect();
        virtual ~SnowEffect();
        virtual void reconfigure( ReconfigureFlags );
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();

    private slots:
        void toggle();
    private:
        void loadTexture();
        GLTexture* texture;
        QList<SnowFlake>* flakes;
        QTime lastFlakeTime;
        long nextFlakeMillis;
        int mNumberFlakes;
        int mMinFlakeSize;
        int mMaxFlakeSize;
        int mMaxVSpeed;
        int mMaxHSpeed;
        bool active;
    };

} // namespace

#endif
