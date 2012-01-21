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

#include "../workspace.h"
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
    QScriptEngine* centralEngine;

    Q_DISABLE_COPY(Workspace)

signals:
    void desktopPresenceChanged(KWin::Client*, int);
    void currentDesktopChanged(int);
    void clientAdded(KWin::Client*);
    void clientRemoved(KWin::Client*);
    void clientManaging(KWin::Client*);
    void clientMinimized(KWin::Client*);
    void clientUnminimized(KWin::Client*);
    void clientRestored(KWin::Client*);
    void clientMaximizeSet(KWin::Client*, bool, bool);
    void killWindowCalled(KWin::Client*);
    void clientActivated(KWin::Client*);
    void clientFullScreenSet(KWin::Client*, bool, bool);
    void clientSetKeepAbove(KWin::Client*, bool);

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

QScriptValue valueForClient(KWin::Client *client, QScriptEngine *engine);

}

#endif
