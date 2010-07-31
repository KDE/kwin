/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2010 Martin Gräßlin <kde@martin-graesslin.com>

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

#ifndef KWIN_SCREENSHOT_H
#define KWIN_SCREENSHOT_H

#include <kwineffects.h>
#include <QObject>

namespace KWin
{

class ScreenShotEffect : public QObject, public Effect
    {
    Q_OBJECT
    public:
        ScreenShotEffect();
        virtual ~ScreenShotEffect();
        virtual void postPaintScreen();

        static bool supported();
        static void convertFromGLImage(QImage &img, int w, int h);
    public Q_SLOTS:
        void screenshot();

    private:
        EffectWindow *m_scheduledScreenshot;
    };

} // namespace

#endif // KWIN_SCREENSHOT_H
