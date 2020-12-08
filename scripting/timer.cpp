/*
    SPDX-FileCopyrightText: 2007 Richard J. Moore <rich@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include <QtScript/QScriptValue>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptContext>
#include <QtScript/QScriptable>
#include <QTimer>

Q_DECLARE_METATYPE(QTimer*)

static QScriptValue newTimer(QScriptEngine *eng, QTimer *timer)
{
    return eng->newQObject(timer, QScriptEngine::AutoOwnership);
}

static QScriptValue ctor(QScriptContext *ctx, QScriptEngine *eng)
{
    return newTimer(eng, new QTimer(qscriptvalue_cast<QObject*>(ctx->argument(0))));
}

QScriptValue constructTimerClass(QScriptEngine *eng)
{
    QScriptValue proto = newTimer(eng, new QTimer());
    eng->setDefaultPrototype(qMetaTypeId<QTimer*>(), proto);

    return eng->newFunction(ctor, proto);
}
