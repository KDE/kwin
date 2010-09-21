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

#include "windowinfo.h"

SWrapper::WindowInfo::WindowInfo(const KWindowInfo info, QScriptEngine* eng, QObject* parent) :
    QObject(parent)
    {
    this->infoClass = info;
    centralEngine = eng;
    }

MAP_GET(valid, bool)
MAP_GET(state, int)
MAP_GET(isMinimized, bool)
MAP_GET(visibleName, QString)
MAP_GET2(windowClassClass, QString)
MAP_GET2(windowClassName, QString)
MAP_GET2(windowRole, QString)

QScriptValue SWrapper::WindowInfo::generate(const KWindowInfo info, QScriptEngine* eng, KWin::Client* client)
    {
    SWrapper::WindowInfo* winInfo = new SWrapper::WindowInfo(info, eng, static_cast<QObject*>(client));

    QScriptValue temp = eng->newQObject(winInfo,
                                        QScriptEngine::ScriptOwnership,
                                        QScriptEngine::ExcludeSuperClassContents | QScriptEngine::ExcludeDeleteLater
                                       );

    KWin::MetaScripting::RefWrapping::embed<SWrapper::WindowInfo*>(temp, winInfo);

    return temp;
    }
