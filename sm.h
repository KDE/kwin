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

#ifndef KWIN_SM_H
#define KWIN_SM_H

#include <QDataStream>
#include <kapplication.h>
#include <ksessionmanager.h>
#include <netwm_def.h>
#include <QRect>

#include <X11/SM/SMlib.h>
#include <fixx11h.h>

class QSocketNotifier;

namespace KWin
{

class Client;

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
    int clientGroup; // Unique identifier for the client group that this window is in

    Client* clientGroupClient; // The first client created that has an identical identifier
    QStringList activities;
};


enum SMSavePhase {
    SMSavePhase0,     // saving global state in "phase 0"
    SMSavePhase2,     // saving window state in phase 2
    SMSavePhase2Full  // complete saving in phase2, there was no phase 0
};

class SessionSaveDoneHelper
    : public QObject
{
    Q_OBJECT
public:
    SessionSaveDoneHelper();
    virtual ~SessionSaveDoneHelper();
    SmcConn connection() const {
        return conn;
    }
    void saveDone();
    void close();
private slots:
    void processData();
private:
    QSocketNotifier* notifier;
    SmcConn conn;
};


class SessionManager
    : public KSessionManager
{
public:
    virtual bool saveState(QSessionManager& sm);
    virtual bool commitData(QSessionManager& sm);
};

} // namespace

#endif
