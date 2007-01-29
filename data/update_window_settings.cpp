/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2004 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

// updates per-window settings from KDE3.2 to KDE3.3

#include <netwm_def.h>
#include <kconfig.h>
#include <kcomponentdata.h>
#include <QRect>
#include <QByteArray>
#include <QList>
#include <QtDBus/QtDBus>
struct SessionInfo
    {
    QByteArray sessionId;
    QByteArray windowRole;
    QByteArray wmCommand;
    QByteArray wmClientMachine;
    QByteArray resourceName;
    QByteArray resourceClass;

    QRect geometry;
    QRect restore;
    QRect fsrestore;
    int maximized;
    int fullscreen;
    int desktop;
    bool minimized;
    bool onAllDesktops;
    bool shaded;
    bool keepAbove;
    bool keepBelow;
    bool skipTaskbar;
    bool skipPager;
    bool userNoBorder;
    NET::WindowType windowType;
    bool active; // means 'was active in the saved session', not used otherwise
    bool fake; // fake session, i.e. 'save window settings', not SM restored
    };

QList<SessionInfo*> fakeSession;

static const char* const window_type_names[] = 
    {
    "Unknown", "Normal" , "Desktop", "Dock", "Toolbar", "Menu", "Dialog",
    "Override", "TopMenu", "Utility", "Splash"
    };
    // change also the two functions below when adding new entries

NET::WindowType txtToWindowType( const char* txt )
    {
    for( int i = NET::Unknown;
         i <= NET::Splash;
         ++i )
        if( qstrcmp( txt, window_type_names[ i + 1 ] ) == 0 ) // +1
            return static_cast< NET::WindowType >( i );
    return static_cast< NET::WindowType >( -2 ); // undefined
    }

void loadFakeSessionInfo( KConfig* config )
    {
    fakeSession.clear();
    config->setGroup("FakeSession" );
    int count =  config->readEntry( "count",0 );
    for ( int i = 1; i <= count; i++ ) 
        {
        QString n = QString::number(i);
        SessionInfo* info = new SessionInfo;
        fakeSession.append( info );
        info->windowRole = config->readEntry( QString("windowRole")+n, QString() ).toLatin1();
        info->resourceName = config->readEntry( QString("resourceName")+n, QString() ).toLatin1();
        info->resourceClass = config->readEntry( QString("resourceClass")+n, QString() ).toLower().toLatin1();
        info->wmClientMachine = config->readEntry( QString("clientMachine")+n, QString() ).toLatin1();
        info->geometry = config->readEntry( QString("geometry")+n, QRect() );
        info->restore = config->readEntry( QString("restore")+n, QRect() );
        info->fsrestore = config->readEntry( QString("fsrestore")+n, QRect() );
        info->maximized = config->readEntry( QString("maximize")+n, 0 );
        info->fullscreen = config->readEntry( QString("fullscreen")+n, 0 );
        info->desktop = config->readEntry( QString("desktop")+n, 0 );
        info->minimized = config->readEntry( QString("iconified")+n, false );
        info->onAllDesktops = config->readEntry( QString("sticky")+n, false );
        info->shaded = config->readEntry( QString("shaded")+n, false );
        info->keepAbove = config->readEntry( QString("staysOnTop")+n, false  );
        info->keepBelow = config->readEntry( QString("keepBelow")+n, false  );
        info->skipTaskbar = config->readEntry( QString("skipTaskbar")+n, false  );
        info->skipPager = config->readEntry( QString("skipPager")+n, false  );
        info->userNoBorder = config->readEntry( QString("userNoBorder")+n, false  );
        info->windowType = txtToWindowType( config->readEntry( QString("windowType")+n, QString() ).toLatin1());
        info->active = false;
        info->fake = true;
        }
    config->deleteGroup( "FakeSession" );
    }

void writeRules( KConfig& cfg )
    {
    cfg.setGroup( "General" );
    int pos = cfg.readEntry( "count",0 );

    QList<SessionInfo*>::iterator it;
    for ( it = fakeSession.begin(); it != fakeSession.end(); ++it)
        {
        if( (*it)->resourceName.isEmpty() && (*it)->resourceClass.isEmpty())
            continue;
        ++pos;
        cfg.setGroup( QString::number( pos ));
        cfg.writeEntry( "description", ( const char* ) ( (*it)->resourceClass + " (KDE3.2)" ));
        cfg.writeEntry( "wmclass", ( const char* )( (*it)->resourceName + ' ' + (*it)->resourceClass ));
        cfg.writeEntry( "wmclasscomplete", true );
        cfg.writeEntry( "wmclassmatch", 1 ); // 1 == exact match
        if( !(*it)->windowRole.isEmpty())
            {
            cfg.writeEntry( "windowrole", ( const char* ) (*it)->windowRole );
            cfg.writeEntry( "windowrolematch", 1 );
            }
        if( (*it)->windowType == static_cast< NET::WindowType >( -2 )) { // undefined
            // all types
        }
        if( (*it)->windowType == NET::Unknown )
            cfg.writeEntry( "types", (int)NET::NormalMask );
        else
            cfg.writeEntry( "types", 1 << (*it)->windowType );
        cfg.writeEntry( "position", (*it)->geometry.topLeft());
        cfg.writeEntry( "positionrule", 4 ); // 4 == remember
        cfg.writeEntry( "size", (*it)->geometry.size());
        cfg.writeEntry( "sizerule", 4 );
        cfg.writeEntry( "maximizevert", (*it)->maximized & NET::MaxVert );
        cfg.writeEntry( "maximizevertrule", 4 );
        cfg.writeEntry( "maximizehoriz", (*it)->maximized & NET::MaxHoriz );
        cfg.writeEntry( "maximizehorizrule", 4 );
        cfg.writeEntry( "fullscreen", (*it)->fullscreen );
        cfg.writeEntry( "fullscreenrule", 4 );
        cfg.writeEntry( "desktop", (*it)->desktop );
        cfg.writeEntry( "desktoprule", 4 );
        cfg.writeEntry( "minimize", (*it)->minimized );
        cfg.writeEntry( "minimizerule", 4 );
        cfg.writeEntry( "shade", (*it)->shaded );
        cfg.writeEntry( "shaderule", 4 );
        cfg.writeEntry( "above", (*it)->keepAbove );
        cfg.writeEntry( "aboverule", 4 );
        cfg.writeEntry( "below", (*it)->keepBelow );
        cfg.writeEntry( "belowrule", 4 );
        cfg.writeEntry( "skiptaskbar", (*it)->skipTaskbar );
        cfg.writeEntry( "skiptaskbarrule", 4 );
        cfg.writeEntry( "skippager", (*it)->skipPager );
        cfg.writeEntry( "skippagerrule", 4 );
        cfg.writeEntry( "noborder", (*it)->userNoBorder );
        cfg.writeEntry( "noborderrule", 4 );
        }
    cfg.setGroup( "General" );
    cfg.writeEntry( "count", pos );
    }

int main()
    {
    KComponentData inst( "kwin_update_window_settings" );
    KConfig src_cfg( "kwinrc" );
    KConfig dest_cfg( "kwinrulesrc" );
    loadFakeSessionInfo( &src_cfg );
    writeRules( dest_cfg );
    src_cfg.sync();
    dest_cfg.sync();
    // Send signal to all kwin instances
    QDBusMessage message =
        QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
    }
