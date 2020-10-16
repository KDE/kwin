/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Rohan Prabhu <rohan@rohanprabhu.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_SCRIPTING_META_H
#define KWIN_SCRIPTING_META_H

#include <QScriptValueIterator>

// forward declarations
class QPoint;
class QRect;
class QScriptContext;
class QSize;

namespace KWin {
class AbstractClient;
class Toplevel;
class X11Client;
}

typedef KWin::AbstractClient *KAbstractClientRef;
typedef KWin::X11Client *KClientRef;
typedef KWin::Toplevel* KToplevelRef;

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

namespace AbstractClient
{
QScriptValue toScriptValue(QScriptEngine *engine, const KAbstractClientRef &client);
void fromScriptValue(const QScriptValue &value, KAbstractClientRef &client);
}

namespace X11Client
{
QScriptValue toScriptValue(QScriptEngine *eng, const KClientRef &client);
void fromScriptValue(const QScriptValue &value, KClientRef& client);
}

namespace Toplevel
{
QScriptValue toScriptValue(QScriptEngine *eng, const KToplevelRef &client);
void fromScriptValue(const QScriptValue &value, KToplevelRef& client);
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
