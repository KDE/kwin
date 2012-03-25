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

#include <KDE/KAction>
#include <KDE/KActionCollection>
#include <KDE/KDebug>
#include <QtScript/QScriptEngine>

namespace KWin
{

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
};

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
};

inline void registerGlobalShortcutFunction(QObject *parent, QScriptEngine *engine, QScriptEngine::FunctionSignature function)
{
    QScriptValue shortcutFunc = engine->newFunction(function);
    shortcutFunc.setData(engine->newQObject(parent));
    engine->globalObject().setProperty("registerShortcut", shortcutFunc);
};
} // namespace KWin

#endif // KWIN_SCRIPTINGUTILS_H
