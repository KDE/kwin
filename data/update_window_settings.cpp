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
    KConfigGroup fakeGroup(config, "FakeSession");
    int count = fakeGroup.readEntry( "count",0 );
    for ( int i = 1; i <= count; i++ ) 
        {
        QString n = QString::number(i);
        SessionInfo* info = new SessionInfo;
        fakeSession.append( info );
        info->windowRole = fakeGroup.readEntry( QString("windowRole")+n, QString() ).toLatin1();
        info->resourceName = fakeGroup.readEntry( QString("resourceName")+n, QString() ).toLatin1();
        info->resourceClass = fakeGroup.readEntry( QString("resourceClass")+n, QString() ).toLower().toLatin1();
        info->wmClientMachine = fakeGroup.readEntry( QString("clientMachine")+n, QString() ).toLatin1();
        info->geometry = fakeGroup.readEntry( QString("geometry")+n, QRect() );
        info->restore = fakeGroup.readEntry( QString("restore")+n, QRect() );
        info->fsrestore = fakeGroup.readEntry( QString("fsrestore")+n, QRect() );
        info->maximized = fakeGroup.readEntry( QString("maximize")+n, 0 );
        info->fullscreen = fakeGroup.readEntry( QString("fullscreen")+n, 0 );
        info->desktop = fakeGroup.readEntry( QString("desktop")+n, 0 );
        info->minimized = fakeGroup.readEntry( QString("iconified")+n, false );
        info->onAllDesktops = fakeGroup.readEntry( QString("sticky")+n, false );
        info->shaded = fakeGroup.readEntry( QString("shaded")+n, false );
        info->keepAbove = fakeGroup.readEntry( QString("staysOnTop")+n, false  );
        info->keepBelow = fakeGroup.readEntry( QString("keepBelow")+n, false  );
        info->skipTaskbar = fakeGroup.readEntry( QString("skipTaskbar")+n, false  );
        info->skipPager = fakeGroup.readEntry( QString("skipPager")+n, false  );
        info->userNoBorder = fakeGroup.readEntry( QString("userNoBorder")+n, false  );
        info->windowType = txtToWindowType( fakeGroup.readEntry( QString("windowType")+n, QString() ).toLatin1());
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
        KConfigGroup groupCfg(&cfg, QString::number( pos ));
        groupCfg.writeEntry( "description", ( const char* ) ( (*it)->resourceClass + " (KDE3.2)" ));
        groupCfg.writeEntry( "wmclass", ( const char* )( (*it)->resourceName + ' ' + (*it)->resourceClass ));
        groupCfg.writeEntry( "wmclasscomplete", true );
        groupCfg.writeEntry( "wmclassmatch", 1 ); // 1 == exact match
        if( !(*it)->windowRole.isEmpty())
            {
            groupCfg.writeEntry( "windowrole", ( const char* ) (*it)->windowRole );
            groupCfg.writeEntry( "windowrolematch", 1 );
            }
        if( (*it)->windowType == static_cast< NET::WindowType >( -2 )) { // undefined
            // all types
        }
        if( (*it)->windowType == NET::Unknown )
            groupCfg.writeEntry( "types", (int)NET::NormalMask );
        else
            groupCfg.writeEntry( "types", 1 << (*it)->windowType );
        groupCfg.writeEntry( "position", (*it)->geometry.topLeft());
        groupCfg.writeEntry( "positionrule", 4 ); // 4 == remember
        groupCfg.writeEntry( "size", (*it)->geometry.size());
        groupCfg.writeEntry( "sizerule", 4 );
        groupCfg.writeEntry( "maximizevert", (*it)->maximized & NET::MaxVert );
        groupCfg.writeEntry( "maximizevertrule", 4 );
        groupCfg.writeEntry( "maximizehoriz", (*it)->maximized & NET::MaxHoriz );
        groupCfg.writeEntry( "maximizehorizrule", 4 );
        groupCfg.writeEntry( "fullscreen", (*it)->fullscreen );
        groupCfg.writeEntry( "fullscreenrule", 4 );
        groupCfg.writeEntry( "desktop", (*it)->desktop );
        groupCfg.writeEntry( "desktoprule", 4 );
        groupCfg.writeEntry( "minimize", (*it)->minimized );
        groupCfg.writeEntry( "minimizerule", 4 );
        groupCfg.writeEntry( "shade", (*it)->shaded );
        groupCfg.writeEntry( "shaderule", 4 );
        groupCfg.writeEntry( "above", (*it)->keepAbove );
        groupCfg.writeEntry( "aboverule", 4 );
        groupCfg.writeEntry( "below", (*it)->keepBelow );
        groupCfg.writeEntry( "belowrule", 4 );
        groupCfg.writeEntry( "skiptaskbar", (*it)->skipTaskbar );
        groupCfg.writeEntry( "skiptaskbarrule", 4 );
        groupCfg.writeEntry( "skippager", (*it)->skipPager );
        groupCfg.writeEntry( "skippagerrule", 4 );
        groupCfg.writeEntry( "noborder", (*it)->userNoBorder );
        groupCfg.writeEntry( "noborderrule", 4 );
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
