/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

#include "sm.h"

#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <fixx11h.h>
#include <kconfig.h>
#include <kglobal.h>

#include "workspace.h"
#include "client.h"
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QSocketNotifier>
#include <QSessionManager>
#include <kdebug.h>
#ifdef KWIN_BUILD_TILING
#include "tiling/tiling.h"
#endif

namespace KWin
{

bool SessionManager::saveState(QSessionManager& sm)
{
    // If the session manager is ksmserver, save stacking
    // order, active window, active desktop etc. in phase 1,
    // as ksmserver assures no interaction will be done
    // before the WM finishes phase 1. Saving in phase 2 is
    // too late, as possible user interaction may change some things.
    // Phase2 is still needed though (ICCCM 5.2)
    char* sm_vendor = SmcVendor(static_cast< SmcConn >(sm.handle()));
    bool ksmserver = qstrcmp(sm_vendor, "KDE") == 0;
    free(sm_vendor);
    if (!sm.isPhase2()) {
        Workspace::self()->sessionSaveStarted();
        if (ksmserver)   // save stacking order etc. before "save file?" etc. dialogs change it
            Workspace::self()->storeSession(kapp->sessionConfig(), SMSavePhase0);
        sm.release(); // Qt doesn't automatically release in this case (bug?)
        sm.requestPhase2();
        return true;
    }
    Workspace::self()->storeSession(kapp->sessionConfig(), ksmserver ? SMSavePhase2 : SMSavePhase2Full);
    kapp->sessionConfig()->sync();
    return true;
}

// I bet this is broken, just like everywhere else in KDE
bool SessionManager::commitData(QSessionManager& sm)
{
    if (!sm.isPhase2())
        Workspace::self()->sessionSaveStarted();
    return true;
}

// Workspace

/*!
  Stores the current session in the config file

  \sa loadSessionInfo()
 */
void Workspace::storeSession(KConfig* config, SMSavePhase phase)
{
    KConfigGroup cg(config, "Session");
    int count =  0;
    int active_client = -1;

    if (phase == SMSavePhase2 || phase == SMSavePhase2Full) {
#ifdef KWIN_BUILD_TILING
        cg.writeEntry("tiling", m_tiling->isEnabled());
        if (m_tiling->isEnabled()) {
            kDebug(1212) << "Tiling was ON";
            m_tiling->setEnabled(false);
        }
#endif
    }

    for (ClientList::Iterator it = clients.begin(); it != clients.end(); ++it) {
        Client* c = (*it);
        QByteArray sessionId = c->sessionId();
        QByteArray wmCommand = c->wmCommand();
        if (sessionId.isEmpty())
            // remember also applications that are not XSMP capable
            // and use the obsolete WM_COMMAND / WM_SAVE_YOURSELF
            if (wmCommand.isEmpty())
                continue;
        count++;
        if (c->isActive())
            active_client = count;
        if (phase == SMSavePhase2 || phase == SMSavePhase2Full)
            storeClient(cg, count, c);
    }
    if (phase == SMSavePhase0) {
        // it would be much simpler to save these values to the config file,
        // but both Qt and KDE treat phase1 and phase2 separately,
        // which results in different sessionkey and different config file :(
        session_active_client = active_client;
        session_desktop = currentDesktop();
    } else if (phase == SMSavePhase2) {
        cg.writeEntry("count", count);
        cg.writeEntry("active", session_active_client);
        cg.writeEntry("desktop", session_desktop);
    } else { // SMSavePhase2Full
        cg.writeEntry("count", count);
        cg.writeEntry("active", session_active_client);
        cg.writeEntry("desktop", currentDesktop());
    }
}

void Workspace::storeClient(KConfigGroup &cg, int num, Client *c)
{
    c->setSessionInteract(false); //make sure we get the real values
    QString n = QString::number(num);
    cg.writeEntry(QString("sessionId") + n, c->sessionId().constData());
    cg.writeEntry(QString("windowRole") + n, c->windowRole().constData());
    cg.writeEntry(QString("wmCommand") + n, c->wmCommand().constData());
    cg.writeEntry(QString("wmClientMachine") + n, c->wmClientMachine(true).constData());
    cg.writeEntry(QString("resourceName") + n, c->resourceName().constData());
    cg.writeEntry(QString("resourceClass") + n, c->resourceClass().constData());
    cg.writeEntry(QString("geometry") + n, QRect(c->calculateGravitation(true), c->clientSize()));   // FRAME
    cg.writeEntry(QString("restore") + n, c->geometryRestore());
    cg.writeEntry(QString("fsrestore") + n, c->geometryFSRestore());
    cg.writeEntry(QString("maximize") + n, (int) c->maximizeMode());
    cg.writeEntry(QString("fullscreen") + n, (int) c->fullScreenMode());
    cg.writeEntry(QString("desktop") + n, c->desktop());
    // the config entry is called "iconified" for back. comp. reasons
    // (kconf_update script for updating session files would be too complicated)
    cg.writeEntry(QString("iconified") + n, c->isMinimized());
    cg.writeEntry(QString("opacity") + n, c->opacity());
    // the config entry is called "sticky" for back. comp. reasons
    cg.writeEntry(QString("sticky") + n, c->isOnAllDesktops());
    cg.writeEntry(QString("shaded") + n, c->isShade());
    // the config entry is called "staysOnTop" for back. comp. reasons
    cg.writeEntry(QString("staysOnTop") + n, c->keepAbove());
    cg.writeEntry(QString("keepBelow") + n, c->keepBelow());
    cg.writeEntry(QString("skipTaskbar") + n, c->skipTaskbar(true));
    cg.writeEntry(QString("skipPager") + n, c->skipPager());
    cg.writeEntry(QString("skipSwitcher") + n, c->skipSwitcher());
    // not really just set by user, but name kept for back. comp. reasons
    cg.writeEntry(QString("userNoBorder") + n, c->noBorder());
    cg.writeEntry(QString("windowType") + n, windowTypeToTxt(c->windowType()));
    cg.writeEntry(QString("shortcut") + n, c->shortcut().toString());
    cg.writeEntry(QString("stackingOrder") + n, unconstrained_stacking_order.indexOf(c));
    int group = 0;
    if (c->clientGroup())
        group = c->clientGroup()->clients().count() > 1 ?
                // KConfig doesn't support long so we need to live with less precision on 64-bit systems
                static_cast<int>(reinterpret_cast<long>(c->clientGroup())) : 0;
    cg.writeEntry(QString("clientGroup") + n, group);
    cg.writeEntry(QString("activities") + n, c->activities());
}

void Workspace::storeSubSession(const QString &name, QSet<QByteArray> sessionIds)
{
    //TODO clear it first
    KConfigGroup cg(KGlobal::config(), QString("SubSession: ") + name);
    int count =  0;
    int active_client = -1;
    for (ClientList::Iterator it = clients.begin(); it != clients.end(); ++it) {
        Client* c = (*it);
        QByteArray sessionId = c->sessionId();
        QByteArray wmCommand = c->wmCommand();
        if (sessionId.isEmpty())
            // remember also applications that are not XSMP capable
            // and use the obsolete WM_COMMAND / WM_SAVE_YOURSELF
            if (wmCommand.isEmpty())
                continue;
        if (!sessionIds.contains(sessionId))
            continue;

        kDebug() << "storing" << sessionId;
        count++;
        if (c->isActive())
            active_client = count;
        storeClient(cg, count, c);
    }
    cg.writeEntry("count", count);
    cg.writeEntry("active", active_client);
    //cg.writeEntry( "desktop", currentDesktop());
}

bool Workspace::stopActivity(const QString &id)
{
    if (sessionSaving()) {
        return false; //ksmserver doesn't queue requests (yet)
        //FIXME what about session *loading*?
    }

    //ugly hack to avoid dbus deadlocks
    QMetaObject::invokeMethod(this, "reallyStopActivity", Qt::QueuedConnection, Q_ARG(QString, id));
    //then lie and assume it worked.
    return true;
}

void Workspace::reallyStopActivity(const QString &id)
{
    kDebug() << id;
    const QStringList openActivities = openActivityList();

    QSet<QByteArray> saveSessionIds;
    QSet<QByteArray> dontCloseSessionIds;
    for (ClientList::Iterator it = clients.begin(); it != clients.end(); ++it) {
        Client* c = (*it);
        const QByteArray sessionId = c->sessionId();
        if (sessionId.isEmpty()) {
            continue; //TODO support old wm_command apps too?
        }

        //kDebug() << sessionId;

        //if it's on the activity that's closing, it needs saving
        //but if a process is on some other open activity, I don't wanna close it yet
        //this is, of course, complicated by a process having many windows.
        if (c->isOnAllActivities()) {
            dontCloseSessionIds << sessionId;
            continue;
        }

        const QStringList activities = c->activities();
        foreach (const QString & activityId, activities) {
            if (activityId == id) {
                saveSessionIds << sessionId;
            } else if (openActivities.contains(activityId)) {
                dontCloseSessionIds << sessionId;
            }
        }
    }

    storeSubSession(id, saveSessionIds);

    QStringList saveAndClose;
    QStringList saveOnly;
    foreach (const QByteArray & sessionId, saveSessionIds) {
        if (dontCloseSessionIds.contains(sessionId)) {
            saveOnly << sessionId;
        } else {
            saveAndClose << sessionId;
        }
    }

    kDebug() << "saveActivity" << id << saveAndClose << saveOnly;

    //pass off to ksmserver
    QDBusInterface ksmserver("org.kde.ksmserver", "/KSMServer", "org.kde.KSMServerInterface");
    if (ksmserver.isValid()) {
        ksmserver.asyncCall("saveSubSession", id, saveAndClose, saveOnly);
    } else {
        kDebug() << "couldn't get ksmserver interface";
    }
}

/*!
  Loads the session information from the config file.

  \sa storeSession()
 */
void Workspace::loadSessionInfo()
{
    session.clear();
    KConfigGroup cg(kapp->sessionConfig(), "Session");

#ifdef KWIN_BUILD_TILING
    m_tiling->setEnabled(cg.readEntry("tiling", false));
#endif

    addSessionInfo(cg);
}

void Workspace::addSessionInfo(KConfigGroup &cg)
{
    int count =  cg.readEntry("count", 0);
    int active_client = cg.readEntry("active", 0);
    for (int i = 1; i <= count; i++) {
        QString n = QString::number(i);
        SessionInfo* info = new SessionInfo;
        session.append(info);
        info->sessionId = cg.readEntry(QString("sessionId") + n, QString()).toLatin1();
        info->windowRole = cg.readEntry(QString("windowRole") + n, QString()).toLatin1();
        info->wmCommand = cg.readEntry(QString("wmCommand") + n, QString()).toLatin1();
        info->wmClientMachine = cg.readEntry(QString("wmClientMachine") + n, QString()).toLatin1();
        info->resourceName = cg.readEntry(QString("resourceName") + n, QString()).toLatin1();
        info->resourceClass = cg.readEntry(QString("resourceClass") + n, QString()).toLower().toLatin1();
        info->geometry = cg.readEntry(QString("geometry") + n, QRect());
        info->restore = cg.readEntry(QString("restore") + n, QRect());
        info->fsrestore = cg.readEntry(QString("fsrestore") + n, QRect());
        info->maximized = cg.readEntry(QString("maximize") + n, 0);
        info->fullscreen = cg.readEntry(QString("fullscreen") + n, 0);
        info->desktop = cg.readEntry(QString("desktop") + n, 0);
        info->minimized = cg.readEntry(QString("iconified") + n, false);
        info->opacity = cg.readEntry(QString("opacity") + n, 1.0);
        info->onAllDesktops = cg.readEntry(QString("sticky") + n, false);
        info->shaded = cg.readEntry(QString("shaded") + n, false);
        info->keepAbove = cg.readEntry(QString("staysOnTop") + n, false);
        info->keepBelow = cg.readEntry(QString("keepBelow") + n, false);
        info->skipTaskbar = cg.readEntry(QString("skipTaskbar") + n, false);
        info->skipPager = cg.readEntry(QString("skipPager") + n, false);
        info->skipSwitcher = cg.readEntry(QString("skipSwitcher") + n, false);
        info->noBorder = cg.readEntry(QString("userNoBorder") + n, false);
        info->windowType = txtToWindowType(cg.readEntry(QString("windowType") + n, QString()).toLatin1());
        info->shortcut = cg.readEntry(QString("shortcut") + n, QString());
        info->active = (active_client == i);
        info->stackingOrder = cg.readEntry(QString("stackingOrder") + n, -1);
        info->clientGroup = cg.readEntry(QString("clientGroup") + n, 0);
        info->clientGroupClient = NULL;
        info->activities = cg.readEntry(QString("activities") + n, QStringList());
    }
}

void Workspace::loadSubSessionInfo(const QString &name)
{
    KConfigGroup cg(KGlobal::config(), QString("SubSession: ") + name);
    addSessionInfo(cg);
}

bool Workspace::startActivity(const QString &id)
{
    if (sessionSaving()) {
        return false; //ksmserver doesn't queue requests (yet)
    }

    if (!allActivities_.contains(id)) {
        return false; //bogus id
    }

    loadSubSessionInfo(id);

    QDBusInterface ksmserver("org.kde.ksmserver", "/KSMServer", "org.kde.KSMServerInterface");
    if (ksmserver.isValid()) {
        ksmserver.asyncCall("restoreSubSession", id);
    } else {
        kDebug() << "couldn't get ksmserver interface";
        return false;
    }
    return true;
}

/*!
  Returns a SessionInfo for client \a c. The returned session
  info is removed from the storage. It's up to the caller to delete it.

  This function is called when a new window is mapped and must be managed.
  We try to find a matching entry in the session.

  May return 0 if there's no session info for the client.
 */
SessionInfo* Workspace::takeSessionInfo(Client* c)
{
    SessionInfo *realInfo = 0;
    QByteArray sessionId = c->sessionId();
    QByteArray windowRole = c->windowRole();
    QByteArray wmCommand = c->wmCommand();
    QByteArray wmClientMachine = c->wmClientMachine(true);
    QByteArray resourceName = c->resourceName();
    QByteArray resourceClass = c->resourceClass();

    // First search ``session''
    if (! sessionId.isEmpty()) {
        // look for a real session managed client (algorithm suggested by ICCCM)
        foreach (SessionInfo * info, session) {
            if (realInfo)
                break;
            if (info->sessionId == sessionId && sessionInfoWindowTypeMatch(c, info)) {
                if (! windowRole.isEmpty()) {
                    if (info->windowRole == windowRole) {
                        realInfo = info;
                        session.removeAll(info);
                    }
                } else {
                    if (info->windowRole.isEmpty()
                            && info->resourceName == resourceName
                            && info->resourceClass == resourceClass) {
                        realInfo = info;
                        session.removeAll(info);
                    }
                }
            }
        }
    } else {
        // look for a sessioninfo with matching features.
        foreach (SessionInfo * info, session) {
            if (realInfo)
                break;
            if (info->resourceName == resourceName
                    && info->resourceClass == resourceClass
                    && info->wmClientMachine == wmClientMachine
                    && sessionInfoWindowTypeMatch(c, info)) {
                if (wmCommand.isEmpty() || info->wmCommand == wmCommand) {
                    realInfo = info;
                    session.removeAll(info);
                }
            }
        }
    }

    // Set clientGroupClient for other clients in the same group
    if (realInfo && realInfo->clientGroup)
        foreach (SessionInfo * info, session)
        if (!info->clientGroupClient && info->clientGroup == realInfo->clientGroup)
            info->clientGroupClient = c;

    return realInfo;
}

bool Workspace::sessionInfoWindowTypeMatch(Client* c, SessionInfo* info)
{
    if (info->windowType == -2) {
        // undefined (not really part of NET::WindowType)
        return !c->isSpecialWindow();
    }
    return info->windowType == c->windowType();
}

static const char* const window_type_names[] = {
    "Unknown", "Normal" , "Desktop", "Dock", "Toolbar", "Menu", "Dialog",
    "Override", "TopMenu", "Utility", "Splash"
};
// change also the two functions below when adding new entries

const char* Workspace::windowTypeToTxt(NET::WindowType type)
{
    if (type >= NET::Unknown && type <= NET::Splash)
        return window_type_names[ type + 1 ]; // +1 (unknown==-1)
    if (type == -2)   // undefined (not really part of NET::WindowType)
        return "Undefined";
    kFatal(1212) << "Unknown Window Type" ;
    return NULL;
}

NET::WindowType Workspace::txtToWindowType(const char* txt)
{
    for (int i = NET::Unknown;
            i <= NET::Splash;
            ++i)
        if (qstrcmp(txt, window_type_names[ i + 1 ]) == 0)     // +1
            return static_cast< NET::WindowType >(i);
    return static_cast< NET::WindowType >(-2);   // undefined
}




// KWin's focus stealing prevention causes problems with user interaction
// during session save, as it prevents possible dialogs from getting focus.
// Therefore it's temporarily disabled during session saving. Start of
// session saving can be detected in SessionManager::saveState() above,
// but Qt doesn't have API for saying when session saved finished (either
// successfully, or was canceled). Therefore, create another connection
// to session manager, that will provide this information.
// Similarly the remember feature of window-specific settings should be disabled
// during KDE shutdown when windows may move e.g. because of Kicker going away
// (struts changing). When session saving starts, it can be cancelled, in which
// case the shutdown_cancelled callback is invoked, or it's a checkpoint that
// is immediatelly followed by save_complete, or finally it's a shutdown that
// is immediatelly followed by die callback. So getting save_yourself with shutdown
// set disables window-specific settings remembering, getting shutdown_cancelled
// re-enables, otherwise KWin will go away after die.
static void save_yourself(SmcConn conn_P, SmPointer ptr, int, Bool shutdown, int, Bool)
{
    SessionSaveDoneHelper* session = reinterpret_cast< SessionSaveDoneHelper* >(ptr);
    if (conn_P != session->connection())
        return;
    if (shutdown)
        Workspace::self()->disableRulesUpdates(true);
    SmcSaveYourselfDone(conn_P, True);
}

static void die(SmcConn conn_P, SmPointer ptr)
{
    SessionSaveDoneHelper* session = reinterpret_cast< SessionSaveDoneHelper* >(ptr);
    if (conn_P != session->connection())
        return;
    // session->saveDone(); we will quit anyway
    session->close();
}

static void save_complete(SmcConn conn_P, SmPointer ptr)
{
    SessionSaveDoneHelper* session = reinterpret_cast< SessionSaveDoneHelper* >(ptr);
    if (conn_P != session->connection())
        return;
    session->saveDone();
}

static void shutdown_cancelled(SmcConn conn_P, SmPointer ptr)
{
    SessionSaveDoneHelper* session = reinterpret_cast< SessionSaveDoneHelper* >(ptr);
    if (conn_P != session->connection())
        return;
    Workspace::self()->disableRulesUpdates(false);   // re-enable
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
    conn = SmcOpenConnection(NULL, 0, 1, 0,
                             SmcSaveYourselfProcMask | SmcDieProcMask | SmcSaveCompleteProcMask
                             | SmcShutdownCancelledProcMask, &calls, NULL, &id, 10, err);
    if (id != NULL)
        free(id);
    if (conn == NULL)
        return; // no SM
    // set the required properties, mostly dummy values
    SmPropValue propvalue[ 5 ];
    SmProp props[ 5 ];
    propvalue[ 0 ].length = sizeof(unsigned char);
    unsigned char value0 = SmRestartNever; // so that this extra SM connection doesn't interfere
    propvalue[ 0 ].value = &value0;
    props[ 0 ].name = const_cast< char* >(SmRestartStyleHint);
    props[ 0 ].type = const_cast< char* >(SmCARD8);
    props[ 0 ].num_vals = 1;
    props[ 0 ].vals = &propvalue[ 0 ];
    struct passwd* entry = getpwuid(geteuid());
    propvalue[ 1 ].length = entry != NULL ? strlen(entry->pw_name) : 0;
    propvalue[ 1 ].value = (SmPointer)(entry != NULL ? entry->pw_name : "");
    props[ 1 ].name = const_cast< char* >(SmUserID);
    props[ 1 ].type = const_cast< char* >(SmARRAY8);
    props[ 1 ].num_vals = 1;
    props[ 1 ].vals = &propvalue[ 1 ];
    propvalue[ 2 ].length = 0;
    propvalue[ 2 ].value = (SmPointer)("");
    props[ 2 ].name = const_cast< char* >(SmRestartCommand);
    props[ 2 ].type = const_cast< char* >(SmLISTofARRAY8);
    props[ 2 ].num_vals = 1;
    props[ 2 ].vals = &propvalue[ 2 ];
    propvalue[ 3 ].length = strlen("kwinsmhelper");
    propvalue[ 3 ].value = (SmPointer)"kwinsmhelper";
    props[ 3 ].name = const_cast< char* >(SmProgram);
    props[ 3 ].type = const_cast< char* >(SmARRAY8);
    props[ 3 ].num_vals = 1;
    props[ 3 ].vals = &propvalue[ 3 ];
    propvalue[ 4 ].length = 0;
    propvalue[ 4 ].value = (SmPointer)("");
    props[ 4 ].name = const_cast< char* >(SmCloneCommand);
    props[ 4 ].type = const_cast< char* >(SmLISTofARRAY8);
    props[ 4 ].num_vals = 1;
    props[ 4 ].vals = &propvalue[ 4 ];
    SmProp* p[ 5 ] = { &props[ 0 ], &props[ 1 ], &props[ 2 ], &props[ 3 ], &props[ 4 ] };
    SmcSetProperties(conn, 5, p);
    notifier = new QSocketNotifier(IceConnectionNumber(SmcGetIceConnection(conn)),
                                   QSocketNotifier::Read, this);
    connect(notifier, SIGNAL(activated(int)), SLOT(processData()));
}

SessionSaveDoneHelper::~SessionSaveDoneHelper()
{
    close();
}

void SessionSaveDoneHelper::close()
{
    if (conn != NULL) {
        delete notifier;
        SmcCloseConnection(conn, 0, NULL);
    }
    conn = NULL;
}

void SessionSaveDoneHelper::processData()
{
    if (conn != NULL)
        IceProcessMessages(SmcGetIceConnection(conn), 0, 0);
}

void Workspace::sessionSaveDone()
{
    session_saving = false;
    //remove sessionInteract flag from all clients
    foreach (Client * c, clients) {
        c->setSessionInteract(false);
    }
}

} // namespace

#include "sm.moc"
