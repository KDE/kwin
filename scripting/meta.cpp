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

#include "meta.h"
#include "client.h"
#include "tabgroup.h"

#include <QtScript/QScriptEngine>

using namespace KWin::MetaScripting;

// Meta for QPoint object
QScriptValue Point::toScriptValue(QScriptEngine* eng, const QPoint& point)
{
    QScriptValue temp = eng->newObject();
    temp.setProperty("x", point.x());
    temp.setProperty("y", point.y());
    return temp;
}

void Point::fromScriptValue(const QScriptValue& obj, QPoint& point)
{
    QScriptValue x = obj.property("x", QScriptValue::ResolveLocal);
    QScriptValue y = obj.property("y", QScriptValue::ResolveLocal);

    if (!x.isUndefined() && !y.isUndefined()) {
        point.setX(x.toInt32());
        point.setY(y.toInt32());
    }
}
// End of meta for QPoint object

// Meta for QSize object
QScriptValue Size::toScriptValue(QScriptEngine* eng, const QSize& size)
{
    QScriptValue temp = eng->newObject();
    temp.setProperty("w", size.width());
    temp.setProperty("h", size.height());
    return temp;
}

void Size::fromScriptValue(const QScriptValue& obj, QSize& size)
{
    QScriptValue w = obj.property("w", QScriptValue::ResolveLocal);
    QScriptValue h = obj.property("h", QScriptValue::ResolveLocal);

    if (!w.isUndefined() && !h.isUndefined()) {
        size.setWidth(w.toInt32());
        size.setHeight(h.toInt32());
    }
}
// End of meta for QSize object

// Meta for QRect object. Just a temporary measure, hope to
// add a much better wrapping of the QRect object soon
QScriptValue Rect::toScriptValue(QScriptEngine* eng, const QRect& rect)
{
    QScriptValue temp = eng->newObject();
    temp.setProperty("x", rect.x());
    temp.setProperty("y", rect.y());
    temp.setProperty("width", rect.width());
    temp.setProperty("height", rect.height());

    return temp;
}

void Rect::fromScriptValue(const QScriptValue& obj, QRect &rect)
{
    QScriptValue w = obj.property("width", QScriptValue::ResolveLocal);
    QScriptValue h = obj.property("height", QScriptValue::ResolveLocal);
    QScriptValue x = obj.property("x", QScriptValue::ResolveLocal);
    QScriptValue y = obj.property("y", QScriptValue::ResolveLocal);

    if (!w.isUndefined() && !h.isUndefined() && !x.isUndefined() && !y.isUndefined()) {
        rect.setX(x.toInt32());
        rect.setY(y.toInt32());
        rect.setWidth(w.toInt32());
        rect.setHeight(h.toInt32());
    }
}
// End of meta for QRect object

QScriptValue Client::toScriptValue(QScriptEngine *eng, const KClientRef &client)
{
    return eng->newQObject(client, QScriptEngine::QtOwnership,
                           QScriptEngine::ExcludeChildObjects | QScriptEngine::ExcludeDeleteLater | QScriptEngine::PreferExistingWrapperObject);
}

void Client::fromScriptValue(const QScriptValue &value, KWin::Client* &client)
{
    client = qobject_cast<KWin::Client*>(value.toQObject());
}

// Other helper functions
void KWin::MetaScripting::registration(QScriptEngine* eng)
{
    qScriptRegisterMetaType<QPoint>(eng, Point::toScriptValue, Point::fromScriptValue);
    qScriptRegisterMetaType<QSize>(eng, Size::toScriptValue, Size::fromScriptValue);
    qScriptRegisterMetaType<QRect>(eng, Rect::toScriptValue, Rect::fromScriptValue);
    qScriptRegisterMetaType<KClientRef>(eng, Client::toScriptValue, Client::fromScriptValue);

    qScriptRegisterSequenceMetaType<QStringList>(eng);
    qScriptRegisterSequenceMetaType< QList<KWin::Client*> >(eng);
}

QScriptValue KWin::MetaScripting::configExists(QScriptContext* ctx, QScriptEngine* eng)
{
    QHash<QString, QVariant> scriptConfig = (((ctx->thisObject()).data()).toVariant()).toHash();
    QVariant val = scriptConfig.value((ctx->argument(0)).toString(), QVariant());

    return eng->toScriptValue<bool>(val.isValid());
}

QScriptValue KWin::MetaScripting::getConfigValue(QScriptContext* ctx, QScriptEngine* eng)
{
    int num = ctx->argumentCount();
    QHash<QString, QVariant> scriptConfig = (((ctx->thisObject()).data()).toVariant()).toHash();

    /*
     * Handle config.get() separately. Compute and return immediately.
     **/
    if (num == 0) {
        QHash<QString, QVariant>::const_iterator i;
        QScriptValue ret = eng->newArray();

        for (i = scriptConfig.constBegin(); i != scriptConfig.constEnd(); ++i) {
            ret.setProperty(i.key(), eng->newVariant(i.value()));
        }

        return ret;
    }

    if ((num == 1) && !((ctx->argument(0)).isArray())) {
        QVariant val = scriptConfig.value((ctx->argument(0)).toString(), QVariant());

        if (val.isValid()) {
            return eng->newVariant(val);
        } else {
            return QScriptValue();
        }
    } else {
        QScriptValue ret = eng->newArray();
        int j = 0;

        if ((ctx->argument(0)).isArray()) {
            bool simple = (num == 1) ? 0 : (ctx->argument(1)).toBool();
            QScriptValue array = (ctx->argument(0));
            int len = (array.property("length").isValid()) ? array.property("length").toNumber() : 0;

            for (int i = 0; i < len; i++) {
                QVariant val = scriptConfig.value(array.property(i).toString(), QVariant());

                if (val.isValid()) {
                    if (simple) {
                        ret.setProperty(j, eng->newVariant(val));
                    } else {
                        ret.setProperty(array.property(i).toString(), eng->newVariant(val));
                    }

                    j++;
                }
            }
        } else {
            for (int i = 0; i < num; i++) {
                QVariant val = scriptConfig.value((ctx->argument(i)).toString(), QVariant());

                if (val.isValid()) {
                    ret.setProperty((ctx->argument(i)).toString(), eng->newVariant(val));
                    j = 1;
                }
            }
        }


        if (j == 0) {
            return QScriptValue();
        } else {
            return ret;
        }
    }
}

void KWin::MetaScripting::supplyConfig(QScriptEngine* eng, const QVariant& scriptConfig)
{
    QScriptValue configObject = eng->newObject();
    configObject.setData(eng->newVariant(scriptConfig));
    configObject.setProperty("get", eng->newFunction(getConfigValue, 0), QScriptValue::Undeletable);
    configObject.setProperty("exists", eng->newFunction(configExists, 0), QScriptValue::Undeletable);
    configObject.setProperty("loaded", ((scriptConfig.toHash().empty()) ? eng->newVariant((bool)0) : eng->newVariant((bool)1)), QScriptValue::Undeletable);
    (eng->globalObject()).setProperty("config", configObject);
}

void KWin::MetaScripting::supplyConfig(QScriptEngine* eng)
{
    KWin::MetaScripting::supplyConfig(eng, QVariant(QHash<QString, QVariant>()));
}

void KWin::MetaScripting::valueMerge(QScriptValue& first, QScriptValue second)
{
    QScriptValueIterator value_it(second);

    while (value_it.hasNext()) {
        value_it.next();
        first.setProperty(value_it.name(), value_it.value());
    }
}
