/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2005 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

// read additional window rules and add them to kwinrulesrc

#include <kconfig.h>
#include <kconfiggroup.h>
#include <kdebug.h>
#include <kcomponentdata.h>
#include <kstandarddirs.h>
#include <QtDBus/QtDBus>

int main( int argc, char* argv[] )
    {
    if( argc != 2 )
        return 1;
    KComponentData inst( "kwin_update_default_rules" );
    QString file = KStandardDirs::locate( "data", QString( "kwin/default_rules/" ) + argv[ 1 ] );
    if( file.isEmpty())
        {
        kWarning() << "File " << argv[ 1 ] << " not found!" ;
        return 1;
        }
    KConfig src_cfg( file );
    KConfig dest_cfg( "kwinrulesrc" );
	KConfigGroup scg(&src_cfg, "General");
	KConfigGroup dcg(&dest_cfg, "General");
    int count = scg.readEntry( "count", 0 );
    int pos = dcg.readEntry( "count", 0 );
    for( int group = 1;
         group <= count;
         ++group )
        {
        QMap< QString, QString > entries = src_cfg.entryMap( QString::number( group ));
        ++pos;
        dest_cfg.deleteGroup( QString::number( pos ));
		KConfigGroup dcg2 (&dest_cfg, QString::number( pos ));
        for( QMap< QString, QString >::ConstIterator it = entries.begin();
             it != entries.end();
             ++it )
            dcg2.writeEntry( it.key(), *it );
        }
    dcg.writeEntry( "count", pos );
    scg.sync();
    dcg.sync();
    // Send signal to all kwin instances
    QDBusMessage message =
        QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
 
    }
