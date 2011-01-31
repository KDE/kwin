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

#ifndef KWIN_SCRIPTING_TOPLEVEL_H
#define KWIN_SCRIPTING_TOPLEVEL_H

#include "./../toplevel.h"
#include <QObject>
#include <QScriptEngine>

#undef MAP_GET
#define MAP_GET(type, method) \
    QScriptValue SWrapper::Toplevel::method(QScriptContext* ctx, QScriptEngine* eng) { \
        KWin::Toplevel* central = extractClient(ctx->thisObject()); \
        \
        if (central == 0) { \
            return QScriptValue(); \
        } else { \
            return eng->toScriptValue<type>(central->method()); \
        } \
    }

#define MAP_PROTO(method) static QScriptValue method(QScriptContext* ctx, QScriptEngine* eng);

#include <QDebug>

namespace SWrapper
{

class Toplevel : public QObject
{
    Q_OBJECT
protected:
    KWin::Toplevel* tl_centralObject;

private:
    /**
      * Naming abuse. Shoud've been extractToplevel, but let it just be
      * for a while now :)
      */
    static KWin::Toplevel* extractClient(const QScriptValue&);

public:
    //generate a new QScriptValue constructed from a
    //Toplevel object or any of it's derived classes
    static void tl_append(QScriptValue&, QScriptEngine*);

    static QScriptValue pos(QScriptContext* ctx, QScriptEngine* eng);

    // No more properties
    /**
      * The various properties of KWin::Toplevel.
      */
    MAP_PROTO(size)
    MAP_PROTO(width)
    MAP_PROTO(height)
    MAP_PROTO(x)
    MAP_PROTO(y)
    MAP_PROTO(geometry)
    MAP_PROTO(opacity)
    MAP_PROTO(hasAlpha)
    MAP_PROTO(setOpacity)

    Toplevel(KWin::Toplevel*);
};

}

#endif
