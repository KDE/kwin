/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_SCRIPTINGUTILS_H
#define KWIN_SCRIPTINGUTILS_H

#include "input.h"
#include "workspace.h"
#include "screenedge.h"
#include "scripting_logging.h"

#include <KGlobalAccel>
#include <KLocalizedString>
#include <QAction>
#include <QtScript/QScriptEngine>

namespace KWin
{

/**
 * Validates that argument at @p index of given @p context is of required type.
 * Throws a type error in the scripting context if there is a type mismatch.
 * @param context The scripting context in which the argument type needs to be validated.
 * @param index The argument index to validate
 * @returns @c true if the argument is of required type, @c false otherwise
 */
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
 */
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
 */
bool validateParameters(QScriptContext *context, int min, int max);

template<class T>
QScriptValue globalShortcut(QScriptContext *context, QScriptEngine *engine)
{
    T script = qobject_cast<T>(context->callee().data().toQObject());
    if (!script) {
        return engine->undefinedValue();
    }
    if (context->argumentCount() != 4) {
        qCDebug(KWIN_SCRIPTING) << "Incorrect number of arguments! Expected: title, text, keySequence, callback";
        return engine->undefinedValue();
    }
    QAction* a = new QAction(script);
    a->setObjectName(context->argument(0).toString());
    a->setText(context->argument(1).toString());
    const QKeySequence shortcut = QKeySequence(context->argument(2).toString());
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << shortcut);
    script->registerShortcut(a, context->argument(3));
    input()->registerShortcut(shortcut, a);
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
    QScriptValueList arguments;
    arguments << value.engine()->newQObject(a);
    value.call(QScriptValue(), arguments);
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
        ScreenEdges::self()->reserve(static_cast<KWin::ElectricBorder>(edge), script, "borderActivated");
        script->screenEdgeCallbacks().insert(edge, QList<QScriptValue>() << context->argument(1));
    } else {
        it->append(context->argument(1));
    }
    return engine->newVariant(true);
}

template<class T>
QScriptValue registerTouchScreenEdge(QScriptContext *context, QScriptEngine *engine)
{
    auto script = qobject_cast<T>(context->callee().data().toQObject());
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
                                                               "Second argument to registerTouchScreenEdge needs to be a callback"));
    }
    const int edge = context->argument(0).toVariant().toInt();
    const auto ret = script->registerTouchScreenCallback(edge, context->argument(1));
    return engine->newVariant(ret);
}

template<class T>
QScriptValue unregisterTouchScreenEdge(QScriptContext *context, QScriptEngine *engine)
{
    auto script = qobject_cast<T>(context->callee().data().toQObject());
    if (!script) {
        return engine->undefinedValue();
    }
    if (!validateParameters(context, 1, 1)) {
        return engine->undefinedValue();
    }
    if (!validateArgumentType<int>(context)) {
        return engine->undefinedValue();
    }
    const int edge = context->argument(0).toVariant().toInt();
    const auto ret = script->unregisterTouchScreenCallback(edge);
    return engine->newVariant(ret);
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

inline void registerGlobalShortcutFunction(QObject *parent, QScriptEngine *engine, QScriptEngine::FunctionSignature function)
{
    QScriptValue shortcutFunc = engine->newFunction(function);
    shortcutFunc.setData(engine->newQObject(parent));
    engine->globalObject().setProperty(QStringLiteral("registerShortcut"), shortcutFunc);
}

inline void registerScreenEdgeFunction(QObject *parent, QScriptEngine *engine, QScriptEngine::FunctionSignature function)
{
    QScriptValue shortcutFunc = engine->newFunction(function);
    shortcutFunc.setData(engine->newQObject(parent));
    engine->globalObject().setProperty(QStringLiteral("registerScreenEdge"), shortcutFunc);
}

inline void registerTouchScreenEdgeFunction(QObject *parent, QScriptEngine *engine, QScriptEngine::FunctionSignature function)
{
    QScriptValue touchScreenFunc = engine->newFunction(function);
    touchScreenFunc.setData(engine->newQObject(parent));
    engine->globalObject().setProperty(QStringLiteral("registerTouchScreenEdge"), touchScreenFunc);
}

inline void unregisterTouchScreenEdgeFunction(QObject *parent, QScriptEngine *engine, QScriptEngine::FunctionSignature function)
{
    QScriptValue touchScreenFunc = engine->newFunction(function);
    touchScreenFunc.setData(engine->newQObject(parent));
    engine->globalObject().setProperty(QStringLiteral("unregisterTouchScreenEdge"), touchScreenFunc);
}

QVariant dbusToVariant(const QVariant &variant);

} // namespace KWin

#endif // KWIN_SCRIPTINGUTILS_H
