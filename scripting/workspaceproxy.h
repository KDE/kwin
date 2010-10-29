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

#ifndef __KWIN_SCRIPTING_WORKSPACEPROXY_H_
#define __KWIN_SCRIPTING_WORKSPACEPROXY_H_

#include <QObject>
#include <QScriptValue>

#include <QDebug>

#include "../workspace.h"
#include "../client.h"

#define PROXYPASS1(name, param) \
void SWrapper::WorkspaceProxy::sl_ ## name(param _param) { \
    name(_param); \
}

#define PROXYPASS2(name, param1, param2) \
void SWrapper::WorkspaceProxy::sl_ ## name(param1 _param1, param2 _param2) { \
    name(_param1, _param2); \
}

#define PROXYPASS3(name, param1, param2, param3) \
void SWrapper::WorkspaceProxy::sl_ ## name(param1 _param1, param2 _param2, param3 _param3) { \
    name(_param1, _param2, _param3); \
}

typedef QPair<bool, bool> DualBool;

namespace SWrapper
{
/**
  * KWin::WorkspaceProxy simply allows connections from multiple clients to
  * multiple Workspace objects.
  */
class WorkspaceProxy : public QObject
    {
        Q_OBJECT

    public:
        WorkspaceProxy();
        static WorkspaceProxy* _instance;
        static WorkspaceProxy* instance();

    public slots:
        void sl_clientManaging(KWin::Client*);
        void sl_clientMinimized(KWin::Client*);
        void sl_clientUnminimized(KWin::Client*);
        void sl_clientMaximizeSet(KWin::Client*, QPair<bool, bool>);
        void sl_killWindowCalled(KWin::Client*);
        void sl_clientFullScreenSet(KWin::Client*, bool, bool);
        void sl_clientSetKeepAbove(KWin::Client*, bool);

    signals:
        void clientManaging(KWin::Client*);
        void clientMinimized(KWin::Client*);
        void clientUnminimized(KWin::Client*);
        void clientMaximizeSet(KWin::Client*, QPair<bool, bool>);
        void killWindowCalled(KWin::Client*);
        void clientFullScreenSet(KWin::Client*, bool, bool);
        void clientSetKeepAbove(KWin::Client*, bool);
    };

};

#endif
