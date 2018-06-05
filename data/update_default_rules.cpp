/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2005 Lubos Lunak <l.lunak@kde.org>

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

// read additional window rules and add them to kwinrulesrc

#include <kconfig.h>
#include <kconfiggroup.h>
#include <QtDBus>
#include <QStandardPaths>
#include <QDebug>

int main( int argc, char* argv[] )
    {
    if( argc != 2 )
        return 1;

    QCoreApplication::setApplicationName ("kwin_update_default_rules");

    QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QString( "kwin/default_rules/%1" ).arg(argv[ 1 ] ));
    if( file.isEmpty())
        {
        qWarning() << "File " << argv[ 1 ] << " not found!" ;
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
        for( QMap< QString, QString >::ConstIterator it = entries.constBegin();
             it != entries.constEnd();
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
