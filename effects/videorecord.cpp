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

/*

 This effect allows recording a video from the session.
 
 Requires libcaptury:
 
 - svn co svn://77.74.232.49/captury/trunk/capseo
 - you may want to remove 1.10 from AUTOMAKE_OPTIONS in Makefile.am
 - ./autogen.sh
 - the usual configure && make && make install procedure
   (you may want to pass --enable-theora --with-accel=x86 [or amd64])

 - svn co svn://77.74.232.49/captury/trunk/libcaptury
 - you may want to remove 1.10 from AUTOMAKE_OPTIONS in Makefile.am
 - ./autogen.sh
 - the usual configure && make && make install procedure
 
 Video is saved to $HOME/kwin_video.cps, use
 "cpsrecode -i kwin_video.cps  -o - | mplayer -" to play,
 use mencoder the same way to create a video.

*/

#include "videorecord.h"

#include <kaction.h>
#include <kactioncollection.h>
#include <kdebug.h>
#include <klocale.h>
#include <qdir.h>
#include <qfile.h>

namespace KWin
{

KWIN_EFFECT( videorecord, VideoRecordEffect )

VideoRecordEffect::VideoRecordEffect()
    : client( NULL )
    {
    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = static_cast< KAction* >( actionCollection->addAction( "VideoRecord" ));
    a->setText( i18n("Toggle Video Recording" ));
    a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::Key_F11 ));
    connect( a, SIGNAL( triggered( bool )), this, SLOT( toggleRecording()));
    area = QRect( 0, 0, displayWidth(), displayHeight());
    }

VideoRecordEffect::~VideoRecordEffect()
    {
    stopRecording();
    }

void VideoRecordEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
    if( client != NULL )
        capture_region = ( mask & ( PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_TRANSFORMED ))
            ? QRect( 0, 0, displayWidth(), displayHeight()) : region;
    }

void VideoRecordEffect::postPaintScreen()
    {
    effects->postPaintScreen();
    if( client != NULL )
        {
#if 1
        if( CapturyProcessRegionStart( client ) == CAPTURY_SUCCESS )
            {
            capture_region &= QRect( 0, 0, displayWidth(), displayHeight()); // limit to screen
            foreach( QRect r, capture_region.rects())
                {
                int gly = displayHeight() - r.y() - r.height(); // opengl coords
                CapturyProcessRegion( client, r.x(), gly, r.width(), r.height());
                }
            CapturyProcessRegionCommit( client );
            }
#else
        CapturyProcessFrame( client );
#endif
        }
    }

void VideoRecordEffect::startRecording()
    {
    if( client != NULL )
        stopRecording();
    bzero( &config, sizeof( config ));
    config.x = area.x();
    config.y = area.y();
    config.width = area.width();
    config.height = area.height();
    config.scale = 0;
    config.fps = 30; // TODO
    config.deviceType = CAPTURY_DEVICE_GLX; // TODO
    config.deviceHandle = display();
    config.windowHandle = rootWindow(); // TODO
    config.cursor = true;
    client = CapturyOpen( &config );
    if( client == NULL )
        {
        kDebug( 1212 ) << "Video recording init failed";
        return;
        }
    // TODO
    if( CapturySetOutputFileName( client, QFile::encodeName(QDir::homePath()+
                    "/kwin_video.cps").constData() ) != CAPTURY_SUCCESS )
        {
        kDebug( 1212 ) << "Video recording file open failed";
        return;
        }
    effects->addRepaintFull(); // trigger reading initial screen contents into buffer
    kDebug( 1212 ) << "Video recording start";
    }

void VideoRecordEffect::stopRecording()
    {
    if( client == NULL )
        return;
    kDebug( 1212 ) << "Video recording stop";
    CapturyClose( client );
    client = NULL;
    }

void VideoRecordEffect::toggleRecording()
    {
    if( client == NULL )
        startRecording();
    else
        stopRecording();
    }

} // namespace

#include "videorecord.moc"
