/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef KWIN_SCRIPTINGUTILS_H
#define KWIN_SCRIPTINGUTILS_H

#include "workspace.h"

#include <KDE/KAction>
#include <KDE/KActionCollection>
#include <KDE/KDebug>
#include <KDE/KLocalizedString>
#include <QtScript/QScriptEngine>

namespace KWin
{

/**
 * Validates that argument at @p index of given @p context is of required type.
 * Throws a type error in the scripting context if there is a type mismatch.
 * @param context The scripting context in which the argument type needs to be validated.
 * @param index The argument index to validate
 * @returns @c true if the argument is of required type, @c false otherwise
 **/
template<class T>
bool validateArgumentType(QScriptContext *context, int index)
{
    const bool result = context->argument(index).toVariant().canConvert<T>();
    if (!result) {
        context->throwError(QScriptContext::TypeError,
            i18nc("KWin Scripting function received incorrect value for an expected type",
                  "%1 is not of required type", context->argument(index).toString()));
    }
    return result;
}

/**
 * Validates that the argument of @p context is of specified type.
 * Throws a type error in the scripting context if there is a type mismatch.
 * @param context The scripting context in which the argument type needs to be validated.
 * @returns @c true if the argument is of required type, @c false otherwise
 **/
template<class T>
bool validateArgumentType(QScriptContext *context)
{
    return validateArgumentType<T>(context, 0);
}

template<class T, class U>
bool validateArgumentType(QScriptContext *context)
{
    if (!validateArgumentType<T>(context)) {
        return false;
    }
    return validateArgumentType<U>(context, 1);
}

template<class T, class U, class V>
bool validateArgumentType(QScriptContext *context)
{
    if (!validateArgumentType<T, U>(context)) {
        return false;
    }
    return validateArgumentType<V>(context, 2);
}

template<class T, class U, class V, class W>
bool validateArgumentType(QScriptContext *context)
{
    if (!validateArgumentType<T, U, V>(context)) {
        return false;
    }
    return validateArgumentType<W>(context, 3);
}

/**
 * Validates that the argument count of @p context is at least @p min and @p max.
 * Throws a syntax error in the script context if argument count mismatch.
 * @param context The ScriptContext for which the argument count needs to be validated
 * @param min The minimum number of arguments.
 * @param max The maximum number of arguments
 * @returns @c true if the argument count is correct, otherwise @c false
 **/
bool validateParameters(QScriptContext *context, int min, int max);

template<class T>
QScriptValue globalShortcut(QScriptContext *context, QScriptEngine *engine)
{
    T script = qobject_cast<T>(context->callee().data().toQObject());
    if (!script) {
        return engine->undefinedValue();
    }
    if (context->argumentCount() != 4) {
        kDebug(1212) << "Incorrect number of arguments! Expected: title, text, keySequence, callback";
        return engine->undefinedValue();
    }
    KActionCollection* actionCollection = new KActionCollection(script);
    KAction* a = (KAction*)actionCollection->addAction(context->argument(0).toString());
    a->setText(context->argument(1).toString());
    a->setGlobalShortcut(KShortcut(context->argument(2).toString()));
    script->registerShortcut(a, context->argument(3));
    return engine->newVariant(true);
}

template<class T>
void callGlobalShortcutCallback(T script, QObject *sender)
{
    QAction *a = qobject_cast<QAction*>(sender);
    if (!a) {
        return;
    }
    QHash<QAction*, QScriptValue>::const_iterator it = script->shortcutCallbacks().find(a);
    if (it == script->shortcutCallbacks().end()) {
        return;
    }
    QScriptValue value(it.value());
    value.call();
}

template<class T>
QScriptValue registerScreenEdge(QScriptContext *context, QScriptEngine *engine)
{
    T script = qobject_cast<T>(context->callee().data().toQObject());
    if (!script) {
        return engine->undefinedValue();
    }
    if (!validateParameters(context, 2, 2)) {
        return engine->undefinedValue();
    }
    if (!validateArgumentType<int>(context)) {
        return engine->undefinedValue();
    }
    if (!context->argument(1).isFunction()) {
        context->throwError(QScriptContext::SyntaxError, i18nc("KWin Scripting error thrown due to incorrect argument",
                                                               "Second argument to registerScreenEdge needs to be a callback"));
    }

    const int edge = context->argument(0).toVariant().toInt();
    QHash<int, QList<QScriptValue> >::iterator it = script->screenEdgeCallbacks().find(edge);
    if (it == script->screenEdgeCallbacks().end()) {
        // not yet registered
#ifdef KWIN_BUILD_SCREENEDGES
        KWin::Workspace::self()->screenEdge()->reserve(static_cast<KWin::ElectricBorder>(edge));
#endif
        script->screenEdgeCallbacks().insert(edge, QList<QScriptValue>() << context->argument(1));
    } else {
        it->append(context->argument(1));
    }
    return engine->newVariant(true);
}

template<class T>
void screenEdgeActivated(T *script, int edge)
{
    QHash<int, QList<QScriptValue> >::iterator it = script->screenEdgeCallbacks().find(edge);
    if (it != script->screenEdgeCallbacks().end()) {
        foreach (const QScriptValue &value, it.value()) {
            QScriptValue callback(value);
            callback.call();
        }
    }
}

template<class T>
QScriptValue scriptingAssert(QScriptContext *context, QScriptEngine *engine, int min, int max, T defaultVal = T())
{
    if (!validateParameters(context, min, max)) {
        return engine->undefinedValue();
    }
    switch (context->argumentCount()) {
    case 1:
        if (!validateArgumentType<T>(context)) {
            return engine->undefinedValue();
        }
        break;
    case 2:
        if (max == 2) {
            if (!validateArgumentType<T, QString>(context)) {
                return engine->undefinedValue();
            }
        } else {
            if (!validateArgumentType<T, T>(context)) {
                return engine->undefinedValue();
            }
        }
        break;
    case 3:
        if (!validateArgumentType<T, T, QString>(context)) {
            return engine->undefinedValue();
        }
        break;
    }
    if (max == 2) {
        if (context->argument(0).toVariant().value<T>() != defaultVal) {
            if (context->argumentCount() == max) {
                context->throwError(QScriptContext::UnknownError, context->argument(max - 1).toString());
            } else {
                context->throwError(QScriptContext::UnknownError,
                                    i18nc("Assertion failed in KWin script with given value",
                                          "Assertion failed: %1", context->argument(0).toString()));
            }
            return engine->undefinedValue();
        }
    } else {
        if (context->argument(0).toVariant().value<T>() != context->argument(1).toVariant().value<T>()) {
            if (context->argumentCount() == max) {
                context->throwError(QScriptContext::UnknownError, context->argument(max - 1).toString());
            } else {
                context->throwError(QScriptContext::UnknownError,
                                    i18nc("Assertion failed in KWin script with expected value and actual value",
                                          "Assertion failed: Expected %1, got %2",
                                          context->argument(0).toString(), context->argument(1).toString()));
            }
            return engine->undefinedValue();
        }
    }
    return engine->newVariant(true);
}

inline void registerGlobalShortcutFunction(QObject *parent, QScriptEngine *engine, QScriptEngine::FunctionSignature function)
{
    QScriptValue shortcutFunc = engine->newFunction(function);
    shortcutFunc.setData(engine->newQObject(parent));
    engine->globalObject().setProperty("registerShortcut", shortcutFunc);
}

inline void registerScreenEdgeFunction(QObject *parent, QScriptEngine *engine, QScriptEngine::FunctionSignature function)
{
    QScriptValue shortcutFunc = engine->newFunction(function);
    shortcutFunc.setData(engine->newQObject(parent));
    engine->globalObject().setProperty("registerScreenEdge", shortcutFunc);
}
} // namespace KWin

#endif // KWIN_SCRIPTINGUTILS_H
