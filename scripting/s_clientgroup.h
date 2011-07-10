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

#ifndef KWIN_SCRIPTING_S_CLIENTGROUP_H
#define KWIN_SCRIPTING_S_CLIENTGROUP_H

#include <QDebug>
#include <QScriptEngine>
#include "client.h"
#include "clientgroup.h"

namespace SWrapper
{

class ClientGroup : public QObject
{
    Q_OBJECT

private:
    KWin::ClientGroup* centralObject;
    bool invalid;

public:
    ClientGroup(KWin::ClientGroup*);
    ClientGroup(KWin::Client*);

    KWin::ClientGroup* getCentralObject();

    /**
      * A lot of functions for the KWin::ClientGroup* object.
      * Mostly self-explanatory, works exactly like their C++
      * analogues.
      */
    static QScriptValue add(QScriptContext*, QScriptEngine*);
    static QScriptValue remove(QScriptContext*, QScriptEngine*);
    static QScriptValue clients(QScriptContext*, QScriptEngine*);
    static QScriptValue setVisible(QScriptContext*, QScriptEngine*);
    static QScriptValue contains(QScriptContext*, QScriptEngine*);
    static QScriptValue indexOf(QScriptContext*, QScriptEngine*);

    /** This is one weird function. It can be called like:
      * move(client, client) OR move(client, index) OR move(index, client)
      * move(index, index)
      * NOTE: other than move(client, client), all other calls are mapped to
      * move(index, index) using indexof(client)
      */
    static QScriptValue move(QScriptContext*, QScriptEngine*);
    static QScriptValue removeAll(QScriptContext*, QScriptEngine*);
    static QScriptValue closeAll(QScriptContext*, QScriptEngine*);
    static QScriptValue minSize(QScriptContext*, QScriptEngine*);
    static QScriptValue maxSize(QScriptContext*, QScriptEngine*);
    static QScriptValue visible(QScriptContext*, QScriptEngine*);

    static QScriptValue construct(QScriptContext*, QScriptEngine*);
    static QScriptValue generate(QScriptEngine*, SWrapper::ClientGroup*);
    static QScriptValue publishClientGroupClass(QScriptEngine*);
};

}
#endif
