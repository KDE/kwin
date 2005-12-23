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
#include <kinstance.h>
#include <dcopclient.h>
#include <qrect.h>
//Added by qt3to4:
#include <QByteArray>
#include <Q3PtrList>

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

Q3PtrList<SessionInfo> fakeSession;

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
    int count =  config->readNumEntry( "count" );
    for ( int i = 1; i <= count; i++ ) 
        {
        QString n = QString::number(i);
        SessionInfo* info = new SessionInfo;
        fakeSession.append( info );
        info->windowRole = config->readEntry( QString("windowRole")+n ).latin1();
        info->resourceName = config->readEntry( QString("resourceName")+n ).latin1();
        info->resourceClass = config->readEntry( QString("resourceClass")+n ).lower().latin1();
        info->wmClientMachine = config->readEntry( QString("clientMachine")+n ).latin1();
        info->geometry = config->readRectEntry( QString("geometry")+n );
        info->restore = config->readRectEntry( QString("restore")+n );
        info->fsrestore = config->readRectEntry( QString("fsrestore")+n );
        info->maximized = config->readNumEntry( QString("maximize")+n, 0 );
        info->fullscreen = config->readNumEntry( QString("fullscreen")+n, 0 );
        info->desktop = config->readNumEntry( QString("desktop")+n, 0 );
        info->minimized = config->readBoolEntry( QString("iconified")+n, FALSE );
        info->onAllDesktops = config->readBoolEntry( QString("sticky")+n, FALSE );
        info->shaded = config->readBoolEntry( QString("shaded")+n, FALSE );
        info->keepAbove = config->readBoolEntry( QString("staysOnTop")+n, FALSE  );
        info->keepBelow = config->readBoolEntry( QString("keepBelow")+n, FALSE  );
        info->skipTaskbar = config->readBoolEntry( QString("skipTaskbar")+n, FALSE  );
        info->skipPager = config->readBoolEntry( QString("skipPager")+n, FALSE  );
        info->userNoBorder = config->readBoolEntry( QString("userNoBorder")+n, FALSE  );
        info->windowType = txtToWindowType( config->readEntry( QString("windowType")+n ).latin1());
        info->active = false;
        info->fake = true;
        }
    config->deleteGroup( "FakeSession" );
    }

void writeRules( KConfig& cfg )
    {
    cfg.setGroup( "General" );
    int pos = cfg.readNumEntry( "count" );
    for ( SessionInfo* info = fakeSession.first(); info; info = fakeSession.next() )
        {
        if( info->resourceName.isEmpty() && info->resourceClass.isEmpty())
            continue;
        ++pos;
        cfg.setGroup( QString::number( pos ));
        cfg.writeEntry( "description", ( const char* ) ( info->resourceClass + " (KDE3.2)" ));
        cfg.writeEntry( "wmclass", ( const char* )( info->resourceName + ' ' + info->resourceClass ));
        cfg.writeEntry( "wmclasscomplete", true );
        cfg.writeEntry( "wmclassmatch", 1 ); // 1 == exact match
        if( !info->windowRole.isEmpty())
            {
            cfg.writeEntry( "windowrole", ( const char* ) info->windowRole );
            cfg.writeEntry( "windowrolematch", 1 );
            }
        if( info->windowType == static_cast< NET::WindowType >( -2 )) // undefined
            ; // all types
        if( info->windowType == NET::Unknown )
            cfg.writeEntry( "types", NET::NormalMask );
        else
            cfg.writeEntry( "types", 1 << info->windowType );
        cfg.writeEntry( "position", info->geometry.topLeft());
        cfg.writeEntry( "positionrule", 4 ); // 4 == remember
        cfg.writeEntry( "size", info->geometry.size());
        cfg.writeEntry( "sizerule", 4 );
        cfg.writeEntry( "maximizevert", info->maximized & NET::MaxVert );
        cfg.writeEntry( "maximizevertrule", 4 );
        cfg.writeEntry( "maximizehoriz", info->maximized & NET::MaxHoriz );
        cfg.writeEntry( "maximizehorizrule", 4 );
        cfg.writeEntry( "fullscreen", info->fullscreen );
        cfg.writeEntry( "fullscreenrule", 4 );
        cfg.writeEntry( "desktop", info->desktop );
        cfg.writeEntry( "desktoprule", 4 );
        cfg.writeEntry( "minimize", info->minimized );
        cfg.writeEntry( "minimizerule", 4 );
        cfg.writeEntry( "shade", info->shaded );
        cfg.writeEntry( "shaderule", 4 );
        cfg.writeEntry( "above", info->keepAbove );
        cfg.writeEntry( "aboverule", 4 );
        cfg.writeEntry( "below", info->keepBelow );
        cfg.writeEntry( "belowrule", 4 );
        cfg.writeEntry( "skiptaskbar", info->skipTaskbar );
        cfg.writeEntry( "skiptaskbarrule", 4 );
        cfg.writeEntry( "skippager", info->skipPager );
        cfg.writeEntry( "skippagerrule", 4 );
        cfg.writeEntry( "noborder", info->userNoBorder );
        cfg.writeEntry( "noborderrule", 4 );
        }
    cfg.setGroup( "General" );
    cfg.writeEntry( "count", pos );
    }

int main()
    {
    KInstance inst( "kwin_update_window_settings" );
    KConfig src_cfg( "kwinrc" );
    KConfig dest_cfg( "kwinrulesrc" );
    loadFakeSessionInfo( &src_cfg );
    writeRules( dest_cfg );
    src_cfg.sync();
    dest_cfg.sync();
    DCOPClient client;
    client.attach();
    client.send("kwin*", "", "reconfigure()", QByteArray());
    }
