/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_SM_H
#define KWIN_SM_H

#include <QDataStream>
#include <kwinglobals.h>
#include <QStringList>
#include <netwm_def.h>
#include <QRect>

namespace KWin
{

class X11Client;

class SessionManager : public QObject
{
    Q_OBJECT
public:
    SessionManager(QObject *parent);
    ~SessionManager() override;

    SessionState state() const;

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

private:
    void setState(SessionState state);
    SessionState m_sessionState;
};

struct SessionInfo {
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
    bool skipSwitcher;
    bool noBorder;
    NET::WindowType windowType;
    QString shortcut;
    bool active; // means 'was active in the saved session'
    int stackingOrder;
    float opacity;

    QStringList activities;
};


enum SMSavePhase {
    SMSavePhase0,     // saving global state in "phase 0"
    SMSavePhase2,     // saving window state in phase 2
    SMSavePhase2Full  // complete saving in phase2, there was no phase 0
};

} // namespace

#endif
