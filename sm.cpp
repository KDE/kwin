/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "sm.h"

#include <kdebug.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <fixx11h.h>
#include <kconfig.h>
#include <kglobal.h>

#include "workspace.h"
#include "client.h"
#include <qsocketnotifier.h>
#include <qsessionmanager.h>

namespace KWinInternal
{

bool SessionManaged::saveState( QSessionManager& sm )
    {
    // If the session manager is ksmserver, save stacking
    // order, active window, active desktop etc. in phase 1,
    // as ksmserver assures no interaction will be done
    // before the WM finishes phase 1. Saving in phase 2 is
    // too late, as possible user interaction may change some things.
    // Phase2 is still needed though (ICCCM 5.2)
    char* sm_vendor = SmcVendor( static_cast< SmcConn >( sm.handle()));
    bool ksmserver = qstrcmp( sm_vendor, "KDE" ) == 0;
    free( sm_vendor );
    if ( !sm.isPhase2() )
        {
        Workspace::self()->sessionSaveStarted();
        if( ksmserver ) // save stacking order etc. before "save file?" etc. dialogs change it
            Workspace::self()->storeSession( kapp->sessionConfig(), SMSavePhase0 );
        sm.release(); // Qt doesn't automatically release in this case (bug?)
        sm.requestPhase2();
        return true;
        }
    Workspace::self()->storeSession( kapp->sessionConfig(), ksmserver ? SMSavePhase2 : SMSavePhase2Full );
    kapp->sessionConfig()->sync();
    return true;
    }

// I bet this is broken, just like everywhere else in KDE
bool SessionManaged::commitData( QSessionManager& sm )
    {
    if ( !sm.isPhase2() )
        Workspace::self()->sessionSaveStarted();
    return true;
    }

// Workspace

/*!
  Stores the current session in the config file

  \sa loadSessionInfo()
 */
void Workspace::storeSession( KConfig* config, SMSavePhase phase )
    {
    config->setGroup("Session" );
    int count =  0;
    int active_client = -1;
    for (ClientList::Iterator it = clients.begin(); it != clients.end(); ++it) 
        {
        Client* c = (*it);
        QByteArray sessionId = c->sessionId();
        QByteArray wmCommand = c->wmCommand();
        if ( sessionId.isEmpty() )
	    // remember also applications that are not XSMP capable
	    // and use the obsolete WM_COMMAND / WM_SAVE_YOURSELF
            if ( wmCommand.isEmpty() )
                continue;
        count++;
        if( c->isActive())
            active_client = count;
        QString n = QString::number(count);
        if( phase == SMSavePhase2 || phase == SMSavePhase2Full )
            {
            config->writeEntry( QString("sessionId")+n, sessionId.constData() );
            config->writeEntry( QString("windowRole")+n, c->windowRole().constData() );
            config->writeEntry( QString("wmCommand")+n, wmCommand.constData() );
            config->writeEntry( QString("wmClientMachine")+n, c->wmClientMachine( true ).constData() );
            config->writeEntry( QString("resourceName")+n, c->resourceName().constData() );
            config->writeEntry( QString("resourceClass")+n, c->resourceClass().constData() );
            config->writeEntry( QString("geometry")+n,  QRect( c->calculateGravitation(true), c->clientSize() ) ); // FRAME
            config->writeEntry( QString("restore")+n, c->geometryRestore() );
            config->writeEntry( QString("fsrestore")+n, c->geometryFSRestore() );
            config->writeEntry( QString("maximize")+n, (int) c->maximizeMode() );
            config->writeEntry( QString("fullscreen")+n, (int) c->fullScreenMode() );
            config->writeEntry( QString("desktop")+n, c->desktop() );
    	    // the config entry is called "iconified" for back. comp. reasons
            // (kconf_update script for updating session files would be too complicated)
            config->writeEntry( QString("iconified")+n, c->isMinimized() );
            // the config entry is called "sticky" for back. comp. reasons
            config->writeEntry( QString("sticky")+n, c->isOnAllDesktops() );
            config->writeEntry( QString("shaded")+n, c->isShade() );
            // the config entry is called "staysOnTop" for back. comp. reasons
            config->writeEntry( QString("staysOnTop")+n, c->keepAbove() );
            config->writeEntry( QString("keepBelow")+n, c->keepBelow() );
            config->writeEntry( QString("skipTaskbar")+n, c->skipTaskbar( true ) );
            config->writeEntry( QString("skipPager")+n, c->skipPager() );
            config->writeEntry( QString("userNoBorder")+n, c->isUserNoBorder() );
            config->writeEntry( QString("windowType")+n, windowTypeToTxt( c->windowType()));
            config->writeEntry( QString("shortcut")+n, c->shortcut().toStringInternal());
            }
        }
    // TODO store also stacking order
    if( phase == SMSavePhase0 )
        {
        // it would be much simpler to save these values to the config file,
        // but both Qt and KDE treat phase1 and phase2 separately,
        // which results in different sessionkey and different config file :(
        session_active_client = active_client;
        session_desktop = currentDesktop();
        }
    else if( phase == SMSavePhase2 )
        {
        config->writeEntry( "count", count );
        config->writeEntry( "active", session_active_client );
        config->writeEntry( "desktop", session_desktop );
        }
    else // SMSavePhase2Full
        {
        config->writeEntry( "count", count );
        config->writeEntry( "active", session_active_client );
        config->writeEntry( "desktop", currentDesktop());
        }
    }


/*!
  Loads the session information from the config file.

  \sa storeSession()
 */
void Workspace::loadSessionInfo()
    {
    session.clear();
    KConfig* config = kapp->sessionConfig();
    config->setGroup("Session" );
    int count =  config->readEntry( "count",0 );
    int active_client = config->readEntry( "active",0 );
    for ( int i = 1; i <= count; i++ ) 
        {
        QString n = QString::number(i);
        SessionInfo* info = new SessionInfo;
        session.append( info );
        info->sessionId = config->readEntry( QString("sessionId")+n, QString() ).latin1();
        info->windowRole = config->readEntry( QString("windowRole")+n, QString() ).latin1();
        info->wmCommand = config->readEntry( QString("wmCommand")+n, QString() ).latin1();
        info->wmClientMachine = config->readEntry( QString("wmClientMachine")+n, QString() ).latin1();
        info->resourceName = config->readEntry( QString("resourceName")+n, QString() ).latin1();
        info->resourceClass = config->readEntry( QString("resourceClass")+n, QString() ).toLower().latin1();
        info->geometry = config->readEntry( QString("geometry")+n,QRect() );
        info->restore = config->readEntry( QString("restore")+n,QRect() );
        info->fsrestore = config->readEntry( QString("fsrestore")+n,QRect() );
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
        info->windowType = txtToWindowType( config->readEntry( QString("windowType")+n, QString() ).latin1());
        info->shortcut = config->readEntry( QString("shortcut")+n, QString() );
        info->active = ( active_client == i );
        }
    }

/*!
  Returns a SessionInfo for client \a c. The returned session
  info is removed from the storage. It's up to the caller to delete it.

  This function is called when a new window is mapped and must be managed.
  We try to find a matching entry in the session.

  May return 0 if there's no session info for the client.
 */
SessionInfo* Workspace::takeSessionInfo( Client* c )
    {
    SessionInfo *realInfo = 0;
    QByteArray sessionId = c->sessionId();
    QByteArray windowRole = c->windowRole();
    QByteArray wmCommand = c->wmCommand();
    QByteArray wmClientMachine = c->wmClientMachine( true );
    QByteArray resourceName = c->resourceName();
    QByteArray resourceClass = c->resourceClass();

    // First search ``session''
    if (! sessionId.isEmpty() ) 
        {
			// look for a real session managed client (algorithm suggested by ICCCM)
			foreach( SessionInfo* info, session ) {
				if ( realInfo )
					break;
				if ( info->sessionId == sessionId && sessionInfoWindowTypeMatch( c, info )) 
				{
				if ( ! windowRole.isEmpty() ) 
					{
					if ( info->windowRole == windowRole )
						realInfo = info;
						session.remove(info);
					}
				else 
					{
					if ( info->windowRole.isEmpty() &&
						 info->resourceName == resourceName &&
						 info->resourceClass == resourceClass )
						realInfo = info;
						session.remove(info);
					}
				}
			}
        }
    else 
        {
			// look for a sessioninfo with matching features.
			foreach( SessionInfo* info, session ) {
				if ( realInfo ) break;
				if ( info->resourceName == resourceName &&
						info->resourceClass == resourceClass &&
						info->wmClientMachine == wmClientMachine &&
						sessionInfoWindowTypeMatch( c, info ))
					if ( wmCommand.isEmpty() || info->wmCommand == wmCommand ) {
						realInfo = info;
						session.remove( info );
					}
			}
        }

    return realInfo;
    }

bool Workspace::sessionInfoWindowTypeMatch( Client* c, SessionInfo* info )
    {
    if( info->windowType == -2 ) 
        { // undefined (not really part of NET::WindowType)
        return !c->isSpecialWindow();
        }
    return info->windowType == c->windowType();
    }

// maybe needed later
#if 0
// KMainWindow's without name() given have WM_WINDOW_ROLE in the form
// of <appname>-mainwindow#<number>
// when comparing them for fake session info, it's probably better to check
// them without the trailing number
bool Workspace::windowRoleMatch( const QByteArray& role1, const QByteArray& role2 )
    {
    if( role1.isEmpty() && role2.isEmpty())
        return true;
    int pos1 = role1.find( '#' );
    int pos2 = role2.find( '#' );
    bool ret;
    if( pos1 < 0 || pos2 < 0 || pos1 != pos2 )
        ret = role1 == role2;
    else
        ret = qstrncmp( role1, role2, pos1 ) == 0;
    kDebug() << "WR:" << role1 << ":" << pos1 << ":" << role2 << ":" << pos2 << ":::" << ret << endl;
    return ret;
    }
#endif

static const char* const window_type_names[] = 
    {
    "Unknown", "Normal" , "Desktop", "Dock", "Toolbar", "Menu", "Dialog",
    "Override", "TopMenu", "Utility", "Splash"
    };
    // change also the two functions below when adding new entries

const char* Workspace::windowTypeToTxt( NET::WindowType type )
    {
    if( type >= NET::Unknown && type <= NET::Splash )
        return window_type_names[ type + 1 ]; // +1 (unknown==-1)
    if( type == -2 ) // undefined (not really part of NET::WindowType)
        return "Undefined";
    kFatal() << "Unknown Window Type" << endl;
    return NULL;
    }

NET::WindowType Workspace::txtToWindowType( const char* txt )
    {
    for( int i = NET::Unknown;
         i <= NET::Splash;
         ++i )
        if( qstrcmp( txt, window_type_names[ i + 1 ] ) == 0 ) // +1
            return static_cast< NET::WindowType >( i );
    return static_cast< NET::WindowType >( -2 ); // undefined
    }




// KWin's focus stealing prevention causes problems with user interaction
// during session save, as it prevents possible dialogs from getting focus.
// Therefore it's temporarily disabled during session saving. Start of
// session saving can be detected in SessionManaged::saveState() above,
// but Qt doesn't have API for saying when session saved finished (either
// successfully, or was cancelled). Therefore, create another connection
// to session manager, that will provide this information.
static void save_yourself( SmcConn conn_P, SmPointer ptr, int, Bool, int, Bool )
    {
    SessionSaveDoneHelper* session = reinterpret_cast< SessionSaveDoneHelper* >( ptr );
    if( conn_P != session->connection())
        return;
    SmcSaveYourselfDone( conn_P, True );
    }

static void die( SmcConn conn_P, SmPointer ptr )
    {
    SessionSaveDoneHelper* session = reinterpret_cast< SessionSaveDoneHelper* >( ptr );
    if( conn_P != session->connection())
        return;
    // session->saveDone(); we will quit anyway
    session->close();
    }

static void save_complete( SmcConn conn_P, SmPointer ptr )
    {
    SessionSaveDoneHelper* session = reinterpret_cast< SessionSaveDoneHelper* >( ptr );
    if( conn_P != session->connection())
        return;
    session->saveDone();
    }

static void shutdown_cancelled( SmcConn conn_P, SmPointer ptr )
    {
    SessionSaveDoneHelper* session = reinterpret_cast< SessionSaveDoneHelper* >( ptr );
    if( conn_P != session->connection())
        return;
    // no need to differentiate between successful finish and cancel
    session->saveDone();
    }

void SessionSaveDoneHelper::saveDone()
    {
    Workspace::self()->sessionSaveDone();
    }

SessionSaveDoneHelper::SessionSaveDoneHelper()
    {
    SmcCallbacks calls;
    calls.save_yourself.callback = save_yourself;
    calls.save_yourself.client_data = reinterpret_cast< SmPointer >(this);
    calls.die.callback = die;
    calls.die.client_data = reinterpret_cast< SmPointer >(this);
    calls.save_complete.callback = save_complete;
    calls.save_complete.client_data = reinterpret_cast< SmPointer >(this);
    calls.shutdown_cancelled.callback = shutdown_cancelled;
    calls.shutdown_cancelled.client_data = reinterpret_cast< SmPointer >(this);
    char* id = NULL;
    char err[ 11 ];
    conn = SmcOpenConnection( NULL, 0, 1, 0,
        SmcSaveYourselfProcMask | SmcDieProcMask | SmcSaveCompleteProcMask
        | SmcShutdownCancelledProcMask, &calls, NULL, &id, 10, err );
    if( id != NULL )
        free( id );
    if( conn == NULL )
        return; // no SM
    // set the required properties, mostly dummy values
    SmPropValue propvalue[ 5 ];
    SmProp props[ 5 ];
    propvalue[ 0 ].length = sizeof( int );
    int value0 = SmRestartNever; // so that this extra SM connection doesn't interfere
    propvalue[ 0 ].value = &value0;
    props[ 0 ].name = const_cast< char* >( SmRestartStyleHint );
    props[ 0 ].type = const_cast< char* >( SmCARD8 );
    props[ 0 ].num_vals = 1;
    props[ 0 ].vals = &propvalue[ 0 ];
    struct passwd* entry = getpwuid( geteuid() );
    propvalue[ 1 ].length = entry != NULL ? strlen( entry->pw_name ) : 0;
    propvalue[ 1 ].value = (SmPointer)( entry != NULL ? entry->pw_name : "" );
    props[ 1 ].name = const_cast< char* >( SmUserID );
    props[ 1 ].type = const_cast< char* >( SmARRAY8 );
    props[ 1 ].num_vals = 1;
    props[ 1 ].vals = &propvalue[ 1 ];
    propvalue[ 2 ].length = 0;
    propvalue[ 2 ].value = (SmPointer)( "" );
    props[ 2 ].name = const_cast< char* >( SmRestartCommand );
    props[ 2 ].type = const_cast< char* >( SmLISTofARRAY8 );
    props[ 2 ].num_vals = 1;
    props[ 2 ].vals = &propvalue[ 2 ];
    propvalue[ 3 ].length = 0;
    propvalue[ 3 ].value = qApp->argv()[ 0 ];
    props[ 3 ].name = const_cast< char* >( SmProgram );
    props[ 3 ].type = const_cast< char* >( SmARRAY8 );
    props[ 3 ].num_vals = 1;
    props[ 3 ].vals = &propvalue[ 3 ];
    propvalue[ 4 ].length = 0;
    propvalue[ 4 ].value = (SmPointer)( "" );
    props[ 4 ].name = const_cast< char* >( SmCloneCommand );
    props[ 4 ].type = const_cast< char* >( SmLISTofARRAY8 );
    props[ 4 ].num_vals = 1;
    props[ 4 ].vals = &propvalue[ 4 ];
    SmProp* p[ 5 ] = { &props[ 0 ], &props[ 1 ], &props[ 2 ], &props[ 3 ], &props[ 4 ] };
    SmcSetProperties( conn, 5, p );
    notifier = new QSocketNotifier( IceConnectionNumber( SmcGetIceConnection( conn )),
        QSocketNotifier::Read, this );
    connect( notifier, SIGNAL( activated( int )), SLOT( processData()));
    }

SessionSaveDoneHelper::~SessionSaveDoneHelper()
    {
    close();
    }

void SessionSaveDoneHelper::close()
    {
    if( conn != NULL )
        {
        delete notifier;
        SmcCloseConnection( conn, 0, NULL );
        }
    conn = NULL;
    }

void SessionSaveDoneHelper::processData()
    {
    if( conn != NULL )
        IceProcessMessages( SmcGetIceConnection( conn ), 0, 0 );
    }

} // namespace

#include "sm.moc"
