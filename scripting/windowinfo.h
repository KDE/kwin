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

#ifndef KWIN_SCRIPTING_WINDOWINFO_H
#define KWIN_SCRIPTING_WINDOWINFO_H

#include "./../client.h"

#include "meta.h"

#include <kwindowsystem.h>
#include <kwindowinfo.h>

#include <QScriptEngine>

// The following two extremely helpful functions are taken
// from the KWin Scripting proof-of-concept submitted by
// Lubos Lunak <l.lunak@kde.org>. Copyrights belong with
// original author (Lubos Lunak).

#undef MAP_GET
#undef MAP_SET

#define MAP_GET2(name, type2) \
type2 SWrapper::WindowInfo::name() const { \
    return type2(infoClass.name()); \
}

#define MAP_GET(name, type) \
type SWrapper::WindowInfo::name() const { \
    return infoClass.name(); \
}

#define MAP_SET(name, type) \
void SWrapper::Toplevel::name(type value) { \
    subObject->name(value); \
}


namespace SWrapper
{

// No signals/slots here, but the usage of properties
// would make things a breeze, so a QObject it is.
class WindowInfo : public QObject
    {
        Q_OBJECT
    private:
        KWindowInfo infoClass;
        QScriptEngine* centralEngine;

        /**
          * The info about a lot of window properties enumerated from the
          * client this windowinfo was generated from.
          */
        Q_PROPERTY(bool valid READ valid)
        Q_PROPERTY(int state READ state)
        Q_PROPERTY(bool isMinimized READ isMinimized)
        Q_PROPERTY(QString visibleName READ visibleName)
        Q_PROPERTY(QString windowClassClass READ windowClassClass)
        Q_PROPERTY(QString windowClassName READ windowClassName)
        Q_PROPERTY(QString windowRole READ windowRole)

        bool valid() const;
        int state() const;
        bool isMinimized() const;
        QString visibleName() const;

        QString windowClassClass() const;
        QString windowClassName() const;
        QString windowRole() const;

    public:
        WindowInfo(const KWindowInfo, QScriptEngine*, QObject* parent = 0);
        static QScriptValue generate(const KWindowInfo, QScriptEngine*, KWin::Client* client = 0);
    };

}

#endif
