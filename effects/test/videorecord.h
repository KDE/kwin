/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_VIDEORECORD_H
#define KWIN_VIDEORECORD_H

#include <kwineffects.h>

#include <captury/captury.h>

namespace KWin
{

class VideoRecordEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        VideoRecordEffect();
        virtual ~VideoRecordEffect();
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
    private slots:
        void toggleRecording();
    private:
        void startRecording();
        void stopRecording();
        void autoincFilename(QString & url);
        captury_config_t config;
        captury_t* client;
        QRect area;
        QRegion capture_region;
    };

} // namespace

#endif // KWIN_VIDEORECORD_H
