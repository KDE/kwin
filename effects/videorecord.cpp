/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/*

 This effect allows recording a video from the session.
 
 Requires libcaptury:
 
 - svn co svn://battousai.mylair.de/capseo/trunk
 - you may want to remove 1.10 from AUTOMAKE_OPTIONS in Makefile.am
 - ./autogen.sh
 - the usual configure && make && make install procedure
   (you may want to pass --enable-theora --with-accel=x86 [or amd64])

 - svn co svn://battousai.mylair.de/captury/trunk/libcaptury
 - you may want to remove 1.10 from AUTOMAKE_OPTIONS in Makefile.am
 - ./autogen.sh
 - the usual configure && make && make install procedure
 
 Video is saved to /tmp/kwin_video.cps, use
 "cpsrecode -i kwin_video.cps  -o - | mplayer -" to play,
 use mencoder the same way to create a video.

*/

#include <config.h>

#include "videorecord.h"

#include <kaction.h>
#include <kactioncollection.h>
#include <klocale.h>

#include <workspace.h>

#include <GL/gl.h>

namespace KWinInternal
{

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
        capture_region = region;
    }

void VideoRecordEffect::postPaintScreen()
    {
    effects->postPaintScreen();
    if( client != NULL )
        {
#if 1
        if( CapturyProcessRegionStart( client ) == CAPTURY_SUCCESS )
            {
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
        kDebug( 1212 ) << "Video recording init failed" << endl;
        return;
        }
    // TODO
    if( CapturySetOutputFileName( client, "/tmp/kwin_video.cps" ) != CAPTURY_SUCCESS )
        {
        kDebug( 1212 ) << "Video recording file open failed" << endl;
        return;
        }
    workspace()->addRepaintFull(); // trigger reading initial screen contents into buffer
    kDebug( 1212 ) << "Video recording start" << endl;
    }

void VideoRecordEffect::stopRecording()
    {
    if( client == NULL )
        return;
    kDebug( 1212 ) << "Video recording stop" << endl;
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
