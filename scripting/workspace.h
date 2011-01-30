/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Rohan Prabhu <rohan@rohanprabhu.com>

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

#ifndef KWIN_SCRIPTING_WORKSPACE_H
#define KWIN_SCRIPTING_WORKSPACE_H

#include <QDebug>

#include "./../workspace.h"
#include "./../client.h"
#include "./../group.h"

#include "client.h"
#include "workspaceproxy.h"
#include <QScriptEngine>

namespace SWrapper
{

class Workspace : public QObject
{
    Q_OBJECT
    //Don't add WRITE for now. Access considerations
    Q_PROPERTY(int currentDesktop READ currentDesktop)

private:
    static KWin::Workspace* centralObject;
    static SWrapper::Client* clientHolder;
    QScriptEngine* centralEngine;

    Q_DISABLE_COPY(Workspace)

public slots:
    void sl_desktopPresenceChanged(KWin::Client*, int);
    void sl_currentDesktopChanged(int);
    void sl_clientAdded(KWin::Client*);
    void sl_clientRemoved(KWin::Client*);
    void sl_clientManaging(KWin::Client*);
    void sl_clientMinimized(KWin::Client*);
    void sl_clientUnminimized(KWin::Client*);
    void sl_clientMaximizeSet(KWin::Client*, QPair<bool, bool>);
    void sl_killWindowCalled(KWin::Client*);
    void sl_clientActivated(KWin::Client*);
    void sl_groupAdded(KWin::Group*);
    void sl_clientFullScreenSet(KWin::Client*, bool, bool);
    void sl_clientSetKeepAbove(KWin::Client*, bool);

signals:
    void desktopPresenceChanged(QScriptValue, QScriptValue);
    void currentDesktopChanged(QScriptValue);
    void clientAdded(QScriptValue);
    void clientRemoved(QScriptValue);
    void clientManaging(QScriptValue);
    void clientMinimized(QScriptValue);
    void clientUnminimized(QScriptValue);
    void clientRestored(QScriptValue);
    void clientMaximizeSet(QScriptValue, QScriptValue);
    void killWindowCalled(QScriptValue);
    void clientActivated(QScriptValue);
    void clientFullScreenSet(QScriptValue, QScriptValue, QScriptValue);
    void clientSetKeepAbove(QScriptValue, QScriptValue);

public:
    Workspace(QObject* parent = 0);
    int currentDesktop() const;
    void attach(QScriptEngine*);

    static QScriptValue getAllClients(QScriptContext*, QScriptEngine*);

    static bool initialize(KWin::Workspace*);
    static QScriptValue setCurrentDesktop(QScriptContext*, QScriptEngine*);
    static QScriptValue dimensions(QScriptContext*, QScriptEngine*);
    static QScriptValue desktopGridSize(QScriptContext*, QScriptEngine*);
    static QScriptValue activeClient(QScriptContext*, QScriptEngine*);
    static QScriptValue clientGroups(QScriptContext*, QScriptEngine*);
};

}

#endif
