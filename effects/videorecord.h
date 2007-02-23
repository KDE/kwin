/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_VIDEO_H
#define KWIN_VIDEO_H

#include <effects.h>

#include <captury/captury.h>

namespace KWinInternal
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
        captury_config_t config;
        captury_t* client;
        QRect area;
        QRegion capture_region;
    };

} // namespace

#endif
