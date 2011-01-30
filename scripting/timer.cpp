/*
 *   Copyright 2007 Richard J. Moore <rich@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2 as
 *   published by the Free Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <QtScript/QScriptValue>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptContext>
#include <QtScript/QScriptable>
#include <QtCore/QTimer>
#include "plasma-backportglobal.h"

Q_DECLARE_METATYPE(QTimer*)

static QScriptValue newTimer(QScriptEngine *eng, QTimer *timer)
{
    return eng->newQObject(timer, QScriptEngine::AutoOwnership);
}

static QScriptValue ctor(QScriptContext *ctx, QScriptEngine *eng)
{
    return newTimer(eng, new QTimer(qscriptvalue_cast<QObject*>(ctx->argument(0))));
}

static QScriptValue toString(QScriptContext *ctx, QScriptEngine *eng)
{
    DECLARE_SELF(QTimer, toString);
    return QScriptValue(eng, QString::fromLatin1("QTimer(interval=%0)")
                        .arg(self->interval()));
}

static QScriptValue active(QScriptContext *ctx, QScriptEngine *eng)
{
    DECLARE_SELF(QTimer, active);

    if (ctx->argumentCount()) {
        if (ctx->argument(0).toBool()) {
            self->start();
        } else {
            self->stop();
        }
    }

    return QScriptValue(eng, self->isActive());
}

QScriptValue constructTimerClass(QScriptEngine *eng)
{
    QScriptValue proto = newTimer(eng, new QTimer());
    ADD_METHOD(proto, toString);
    eng->setDefaultPrototype(qMetaTypeId<QTimer*>(), proto);
    QScriptValue::PropertyFlags getter = QScriptValue::PropertyGetter;
    QScriptValue::PropertyFlags setter = QScriptValue::PropertySetter;
    proto.setProperty("active", eng->newFunction(active), getter | setter);

    return eng->newFunction(ctor, proto);
}
