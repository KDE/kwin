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

#include "workspaceproxy.h"

SWrapper::WorkspaceProxy* SWrapper::WorkspaceProxy::_instance = 0;

SWrapper::WorkspaceProxy::WorkspaceProxy()
    {
    _instance = this;
    }

SWrapper::WorkspaceProxy* SWrapper::WorkspaceProxy::instance()
    {
    return _instance;
    }

PROXYPASS1(clientManaging, KWin::Client*)
PROXYPASS1(clientMinimized, KWin::Client*)
PROXYPASS1(clientUnminimized, KWin::Client*)
PROXYPASS1(killWindowCalled, KWin::Client*)
PROXYPASS2(clientMaximizeSet, KWin::Client*, DualBool)

PROXYPASS3(clientFullScreenSet, KWin::Client*, bool, bool)
