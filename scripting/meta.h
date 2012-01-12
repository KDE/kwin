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

#ifndef KWIN_SCRIPTING_META_H
#define KWIN_SCRIPTING_META_H

#include <QScriptValueIterator>

// forward declarations
class QPoint;
class QRect;
class QScriptContext;
class QSize;

namespace KWin {
class Client;
}

typedef KWin::Client* KClientRef;

namespace KWin
{
namespace MetaScripting
{


/**
  * The toScriptValue and fromScriptValue functions used in qScriptRegisterMetaType.
  * Conversion functions for QPoint
  */
namespace Point
{
QScriptValue toScriptValue(QScriptEngine*, const QPoint&);
void fromScriptValue(const QScriptValue&, QPoint&);
}

/**
  * The toScriptValue and fromScriptValue functions used in qScriptRegisterMetaType.
  * Conversion functions for QSize
  */
namespace Size
{
QScriptValue toScriptValue(QScriptEngine*, const QSize&);
void fromScriptValue(const QScriptValue&, QSize&);
}

/**
  * The toScriptValue and fromScriptValue functions used in qScriptRegisterMetaType.
  * Conversion functions for QRect
  * TODO: QRect conversions have to be linked from plasma as they provide a lot more
  *       features. As for QSize and QPoint, I don't really plan any such thing.
  */
namespace Rect
{
QScriptValue toScriptValue(QScriptEngine*, const QRect&);
void fromScriptValue(const QScriptValue&, QRect&);
}

namespace Client
{
QScriptValue toScriptValue(QScriptEngine *eng, const KClientRef &client);
void fromScriptValue(const QScriptValue &value, KClientRef& client);
}

/**
  * Merges the second QScriptValue in the first one.
  */
void valueMerge(QScriptValue&, QScriptValue);

/**
  * Registers all the meta conversion to the provided QScriptEngine
  */
void registration(QScriptEngine* eng);

/**
  * Functions for the JS function objects, config.exists and config.get.
  * Read scripting/IMPLIST for details on how they work
  */
QScriptValue configExists(QScriptContext*, QScriptEngine*);
QScriptValue getConfigValue(QScriptContext*, QScriptEngine*);

/**
  * Provide a config object to the given QScriptEngine depending
  * on the keys provided in the QVariant. The provided QVariant
  * MUST returns (true) on isHash()
  */
void supplyConfig(QScriptEngine*, const QVariant&);

/**
  * For engines whose scripts have no associated configuration.
  */
void supplyConfig(QScriptEngine*);

}
}

/**
  * Code linked from plasma for QTimer.
  */
QScriptValue constructTimerClass(QScriptEngine *eng);

#endif
