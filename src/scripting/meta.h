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
class QSize;

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

/**
 * Registers all the meta conversion to the provided QScriptEngine
 */
void registration(QScriptEngine* eng);

}
}

#endif
