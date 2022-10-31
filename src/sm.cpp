/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sm.h"

#include <cstdlib>
#include <kconfig.h>
#include <pwd.h>
#include <unistd.h>

#include "virtualdesktops.h"
#include "workspace.h"
#include "x11window.h"
#include <QDebug>
#include <QSessionManager>

#include "sessionadaptor.h"
#include <QDBusConnection>

namespace KWin
{

static KConfig *sessionConfig(QString id, QString key)
{
    static KConfig *config = nullptr;
    static QString lastId;
    static QString lastKey;
    static QString pattern = QString(QLatin1String("session/%1_%2_%3")).arg(qApp->applicationName());
    if (id != lastId || key != lastKey) {
        delete config;
        config = nullptr;
    }
    lastId = id;
    lastKey = key;
    if (!config) {
        config = new KConfig(pattern.arg(id, key), KConfig::SimpleConfig);
    }
    return config;
}

static const char *const window_type_names[] = {
    "Unknown", "Normal", "Desktop", "Dock", "Toolbar", "Menu", "Dialog",
    "Override", "TopMenu", "Utility", "Splash"};
// change also the two functions below when adding new entries

static const char *windowTypeToTxt(NET::WindowType type)
{
    if (type >= NET::Unknown && type <= NET::Splash) {
        return window_type_names[type + 1]; // +1 (unknown==-1)
    }
    if (type == -2) { // undefined (not really part of NET::WindowType)
        return "Undefined";
    }
    qFatal("Unknown Window Type");
    return nullptr;
}

static NET::WindowType txtToWindowType(const char *txt)
{
    for (int i = NET::Unknown;
         i <= NET::Splash;
         ++i) {
        if (qstrcmp(txt, window_type_names[i + 1]) == 0) { // +1
            return static_cast<NET::WindowType>(i);
        }
    }
    return static_cast<NET::WindowType>(-2); // undefined
}

/**
 * Stores the current session in the config file
 *
 * @see loadSessionInfo
 */
void SessionManager::storeSession(const QString &sessionName, SMSavePhase phase)
{
    qCDebug(KWIN_CORE) << "storing session" << sessionName << "in phase" << phase;
    KConfig *config = sessionConfig(sessionName, QString());

    KConfigGroup cg(config, "Session");
    int count = 0;
    int active_client = -1;

    const QList<X11Window *> x11Clients = workspace()->clientList();
    for (auto it = x11Clients.begin(); it != x11Clients.end(); ++it) {
        X11Window *c = (*it);
        if (c->windowType() > NET::Splash) {
            // window types outside this are not tooltips/menus/OSDs
            // typically these will be unmanaged and not in this list anyway, but that is not enforced
            continue;
        }
        QByteArray sessionId = c->sessionId();
        QString wmCommand = c->wmCommand();
        if (sessionId.isEmpty()) {
            // remember also applications that are not XSMP capable
            // and use the obsolete WM_COMMAND / WM_SAVE_YOURSELF
            if (wmCommand.isEmpty()) {
                continue;
            }
        }
        count++;
        if (c->isActive()) {
            active_client = count;
        }
        if (phase == SMSavePhase2 || phase == SMSavePhase2Full) {
            storeClient(cg, count, c);
        }
    }
    if (phase == SMSavePhase0) {
        // it would be much simpler to save these values to the config file,
        // but both Qt and KDE treat phase1 and phase2 separately,
        // which results in different sessionkey and different config file :(
        m_sessionActiveClient = active_client;
        m_sessionDesktop = VirtualDesktopManager::self()->current();
    } else if (phase == SMSavePhase2) {
        cg.writeEntry("count", count);
        cg.writeEntry("active", m_sessionActiveClient);
        cg.writeEntry("desktop", m_sessionDesktop);
    } else { // SMSavePhase2Full
        cg.writeEntry("count", count);
        cg.writeEntry("active", m_sessionActiveClient);
        cg.writeEntry("desktop", VirtualDesktopManager::self()->current());
    }
    config->sync(); // it previously did some "revert to defaults" stuff for phase1 I think
}

void SessionManager::storeClient(KConfigGroup &cg, int num, X11Window *c)
{
    c->setSessionActivityOverride(false); // make sure we get the real values
    QString n = QString::number(num);
    cg.writeEntry(QLatin1String("sessionId") + n, c->sessionId().constData());
    cg.writeEntry(QLatin1String("windowRole") + n, c->windowRole());
    cg.writeEntry(QLatin1String("wmCommand") + n, c->wmCommand());
    cg.writeEntry(QLatin1String("resourceName") + n, c->resourceName());
    cg.writeEntry(QLatin1String("resourceClass") + n, c->resourceClass());
    cg.writeEntry(QLatin1String("geometry") + n, QRectF(c->calculateGravitation(true), c->clientSize()).toRect()); // FRAME
    cg.writeEntry(QLatin1String("restore") + n, c->geometryRestore());
    cg.writeEntry(QLatin1String("fsrestore") + n, c->fullscreenGeometryRestore());
    cg.writeEntry(QLatin1String("maximize") + n, (int)c->maximizeMode());
    cg.writeEntry(QLatin1String("fullscreen") + n, (int)c->fullScreenMode());
    cg.writeEntry(QLatin1String("desktop") + n, c->desktop());
    // the config entry is called "iconified" for back. comp. reasons
    // (kconf_update script for updating session files would be too complicated)
    cg.writeEntry(QLatin1String("iconified") + n, c->isMinimized());
    cg.writeEntry(QLatin1String("opacity") + n, c->opacity());
    // the config entry is called "sticky" for back. comp. reasons
    cg.writeEntry(QLatin1String("sticky") + n, c->isOnAllDesktops());
    cg.writeEntry(QLatin1String("shaded") + n, c->isShade());
    // the config entry is called "staysOnTop" for back. comp. reasons
    cg.writeEntry(QLatin1String("staysOnTop") + n, c->keepAbove());
    cg.writeEntry(QLatin1String("keepBelow") + n, c->keepBelow());
    cg.writeEntry(QLatin1String("skipTaskbar") + n, c->originalSkipTaskbar());
    cg.writeEntry(QLatin1String("skipPager") + n, c->skipPager());
    cg.writeEntry(QLatin1String("skipSwitcher") + n, c->skipSwitcher());
    // not really just set by user, but name kept for back. comp. reasons
    cg.writeEntry(QLatin1String("userNoBorder") + n, c->userNoBorder());
    cg.writeEntry(QLatin1String("windowType") + n, windowTypeToTxt(c->windowType()));
    cg.writeEntry(QLatin1String("shortcut") + n, c->shortcut().toString());
    cg.writeEntry(QLatin1String("stackingOrder") + n, workspace()->unconstrainedStackingOrder().indexOf(c));
    cg.writeEntry(QLatin1String("activities") + n, c->activities());
}

void SessionManager::storeSubSession(const QString &name, QSet<QByteArray> sessionIds)
{
    // TODO clear it first
    KConfigGroup cg(KSharedConfig::openConfig(), QLatin1String("SubSession: ") + name);
    int count = 0;
    int active_client = -1;
    const QList<X11Window *> x11Clients = workspace()->clientList();

    for (auto it = x11Clients.begin(); it != x11Clients.end(); ++it) {
        X11Window *c = (*it);
        if (c->windowType() > NET::Splash) {
            continue;
        }
        QByteArray sessionId = c->sessionId();
        QString wmCommand = c->wmCommand();
        if (sessionId.isEmpty()) {
            // remember also applications that are not XSMP capable
            // and use the obsolete WM_COMMAND / WM_SAVE_YOURSELF
            if (wmCommand.isEmpty()) {
                continue;
            }
        }
        if (!sessionIds.contains(sessionId)) {
            continue;
        }

        qCDebug(KWIN_CORE) << "storing" << sessionId;
        count++;
        if (c->isActive()) {
            active_client = count;
        }
        storeClient(cg, count, c);
    }
    cg.writeEntry("count", count);
    cg.writeEntry("active", active_client);
    // cg.writeEntry( "desktop", currentDesktop());
}

/**
 * Loads the session information from the config file.
 *
 * @see storeSession
 */
void SessionManager::loadSession(const QString &sessionName)
{
    session.clear();
    KConfigGroup cg(sessionConfig(sessionName, QString()), "Session");
    Q_EMIT loadSessionRequested(sessionName);
    addSessionInfo(cg);
}

void SessionManager::addSessionInfo(KConfigGroup &cg)
{
    workspace()->setInitialDesktop(cg.readEntry("desktop", 1));
    int count = cg.readEntry("count", 0);
    int active_client = cg.readEntry("active", 0);
    for (int i = 1; i <= count; i++) {
        QString n = QString::number(i);
        SessionInfo *info = new SessionInfo;
        session.append(info);
        info->sessionId = cg.readEntry(QLatin1String("sessionId") + n, QString()).toLatin1();
        info->windowRole = cg.readEntry(QLatin1String("windowRole") + n, QString());
        info->wmCommand = cg.readEntry(QLatin1String("wmCommand") + n, QString()).toLatin1();
        info->resourceName = cg.readEntry(QLatin1String("resourceName") + n, QString());
        info->resourceClass = cg.readEntry(QLatin1String("resourceClass") + n, QString()).toLower();
        info->geometry = cg.readEntry(QLatin1String("geometry") + n, QRect());
        info->restore = cg.readEntry(QLatin1String("restore") + n, QRect());
        info->fsrestore = cg.readEntry(QLatin1String("fsrestore") + n, QRect());
        info->maximized = cg.readEntry(QLatin1String("maximize") + n, 0);
        info->fullscreen = cg.readEntry(QLatin1String("fullscreen") + n, 0);
        info->desktop = cg.readEntry(QLatin1String("desktop") + n, 0);
        info->minimized = cg.readEntry(QLatin1String("iconified") + n, false);
        info->opacity = cg.readEntry(QLatin1String("opacity") + n, 1.0);
        info->onAllDesktops = cg.readEntry(QLatin1String("sticky") + n, false);
        info->shaded = cg.readEntry(QLatin1String("shaded") + n, false);
        info->keepAbove = cg.readEntry(QLatin1String("staysOnTop") + n, false);
        info->keepBelow = cg.readEntry(QLatin1String("keepBelow") + n, false);
        info->skipTaskbar = cg.readEntry(QLatin1String("skipTaskbar") + n, false);
        info->skipPager = cg.readEntry(QLatin1String("skipPager") + n, false);
        info->skipSwitcher = cg.readEntry(QLatin1String("skipSwitcher") + n, false);
        info->noBorder = cg.readEntry(QLatin1String("userNoBorder") + n, false);
        info->windowType = txtToWindowType(cg.readEntry(QLatin1String("windowType") + n, QString()).toLatin1().constData());
        info->shortcut = cg.readEntry(QLatin1String("shortcut") + n, QString());
        info->active = (active_client == i);
        info->stackingOrder = cg.readEntry(QLatin1String("stackingOrder") + n, -1);
        info->activities = cg.readEntry(QLatin1String("activities") + n, QStringList());
    }
}

void SessionManager::loadSubSessionInfo(const QString &name)
{
    KConfigGroup cg(KSharedConfig::openConfig(), QLatin1String("SubSession: ") + name);
    addSessionInfo(cg);
}

static bool sessionInfoWindowTypeMatch(X11Window *c, SessionInfo *info)
{
    if (info->windowType == -2) {
        // undefined (not really part of NET::WindowType)
        return !c->isSpecialWindow();
    }
    return info->windowType == c->windowType();
}

/**
 * Returns a SessionInfo for client \a c. The returned session
 * info is removed from the storage. It's up to the caller to delete it.
 *
 * This function is called when a new window is mapped and must be managed.
 * We try to find a matching entry in the session.
 *
 * May return 0 if there's no session info for the client.
 */
SessionInfo *SessionManager::takeSessionInfo(X11Window *c)
{
    SessionInfo *realInfo = nullptr;
    QByteArray sessionId = c->sessionId();
    QString windowRole = c->windowRole();
    QString wmCommand = c->wmCommand();
    QString resourceName = c->resourceName();
    QString resourceClass = c->resourceClass();

    // First search ``session''
    if (!sessionId.isEmpty()) {
        // look for a real session managed client (algorithm suggested by ICCCM)
        for (SessionInfo *info : std::as_const(session)) {
            if (realInfo) {
                break;
            }
            if (info->sessionId == sessionId && sessionInfoWindowTypeMatch(c, info)) {
                if (!windowRole.isEmpty()) {
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
        for (SessionInfo *info : std::as_const(session)) {
            if (realInfo) {
                break;
            }
            if (info->resourceName == resourceName
                && info->resourceClass == resourceClass
                && sessionInfoWindowTypeMatch(c, info)) {
                if (wmCommand.isEmpty() || info->wmCommand == wmCommand) {
                    realInfo = info;
                    session.removeAll(info);
                }
            }
        }
    }
    return realInfo;
}

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
    new SessionAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Session"), this);
}

SessionManager::~SessionManager()
{
    qDeleteAll(session);
}

SessionState SessionManager::state() const
{
    return m_sessionState;
}

void SessionManager::setState(uint state)
{
    switch (state) {
    case 0:
        setState(SessionState::Saving);
        break;
    case 1:
        setState(SessionState::Quitting);
        break;
    default:
        setState(SessionState::Normal);
    }
}

// TODO should we rethink this now that we have dedicated start end end save methods?
void SessionManager::setState(SessionState state)
{
    if (state == m_sessionState) {
        return;
    }
    // If we're starting to save a session
    if (state == SessionState::Saving) {
        workspace()->rulebook()->setUpdatesDisabled(true);
    }
    // If we're ending a save session due to either completion or cancellation
    if (m_sessionState == SessionState::Saving) {
        workspace()->rulebook()->setUpdatesDisabled(false);
        Workspace::self()->forEachClient([](X11Window *client) {
            client->setSessionActivityOverride(false);
        });
    }
    m_sessionState = state;
    Q_EMIT stateChanged();
}

void SessionManager::aboutToSaveSession(const QString &name)
{
    Q_EMIT prepareSessionSaveRequested(name);
    storeSession(name, SMSavePhase0);
}

void SessionManager::finishSaveSession(const QString &name)
{
    Q_EMIT finishSessionSaveRequested(name);
    storeSession(name, SMSavePhase2);
}

void SessionManager::quit()
{
    qApp->quit();
}

} // namespace
