/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDBusContext>
#include <QDataStream>
#include <QRect>
#include <QStringList>
#include <QTimer>

#include <KConfigGroup>

#include "effect/globals.h"

namespace KWin
{

class X11Window;
struct SessionInfo;
class XdgToplevelWindow;

class SessionManager : public QObject, public QDBusContext
{
    Q_OBJECT
public:
    enum SMSavePhase {
        SMSavePhase0, // saving global state in "phase 0"
        SMSavePhase2, // saving window state in phase 2
        SMSavePhase2Full, // complete saving in phase2, there was no phase 0
    };

    SessionManager(QObject *parent);
    ~SessionManager() override;

    SessionState state() const;

    void loadSubSessionInfo(const QString &name);

#if KWIN_BUILD_X11
    void storeSubSession(const QString &name, QSet<QByteArray> sessionIds);
    SessionInfo *takeSessionInfo(X11Window *);
#endif

Q_SIGNALS:
    void stateChanged();

    void loadSessionRequested(const QString &name);
    void prepareSessionSaveRequested(const QString &name);
    void finishSessionSaveRequested(const QString &name);

public Q_SLOTS: // DBus API
    void setState(uint state);
    void loadSession(const QString &name);
    void aboutToSaveSession(const QString &name);
    void finishSaveSession(const QString &name);
    bool closeWaylandWindows();
    void quit();

private:
    void setState(SessionState state);

    void storeSession(const QString &sessionName, SMSavePhase phase);
#if KWIN_BUILD_X11
    void storeClient(KConfigGroup &cg, int num, X11Window *c);
#endif
    void loadSessionInfo(const QString &sessionName);
    void addSessionInfo(KConfigGroup &cg);

    SessionState m_sessionState = SessionState::Normal;

    int m_sessionActiveClient;
    int m_sessionDesktop;

    QList<SessionInfo *> session;
    QList<XdgToplevelWindow *> m_pendingWindows;
    QTimer m_closeTimer;
    std::unique_ptr<QObject> m_closingWindowsGuard;
};

struct SessionInfo
{
    QByteArray sessionId;
    QString windowRole;
    QString wmCommand;
    QString wmClientMachine;
    QString resourceName;
    QString resourceClass;

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
    bool skipSwitcher;
    bool noBorder;
    WindowType windowType;
    QString shortcut;
    bool active; // means 'was active in the saved session'
    int stackingOrder;
    float opacity;

    QStringList activities;
};

} // namespace
