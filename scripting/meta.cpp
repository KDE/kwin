/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Rohan Prabhu <rohan@rohanprabhu.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "meta.h"
#include "x11client.h"

#include <QtScript/QScriptEngine>

using namespace KWin::MetaScripting;

// Meta for QPoint object
QScriptValue Point::toScriptValue(QScriptEngine* eng, const QPoint& point)
{
    QScriptValue temp = eng->newObject();
    temp.setProperty(QStringLiteral("x"), point.x());
    temp.setProperty(QStringLiteral("y"), point.y());
    return temp;
}

void Point::fromScriptValue(const QScriptValue& obj, QPoint& point)
{
    QScriptValue x = obj.property(QStringLiteral("x"), QScriptValue::ResolveLocal);
    QScriptValue y = obj.property(QStringLiteral("y"), QScriptValue::ResolveLocal);

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
    temp.setProperty(QStringLiteral("w"), size.width());
    temp.setProperty(QStringLiteral("h"), size.height());
    return temp;
}

void Size::fromScriptValue(const QScriptValue& obj, QSize& size)
{
    QScriptValue w = obj.property(QStringLiteral("w"), QScriptValue::ResolveLocal);
    QScriptValue h = obj.property(QStringLiteral("h"), QScriptValue::ResolveLocal);

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
    temp.setProperty(QStringLiteral("x"), rect.x());
    temp.setProperty(QStringLiteral("y"), rect.y());
    temp.setProperty(QStringLiteral("width"), rect.width());
    temp.setProperty(QStringLiteral("height"), rect.height());

    return temp;
}

void Rect::fromScriptValue(const QScriptValue& obj, QRect &rect)
{
    QScriptValue w = obj.property(QStringLiteral("width"), QScriptValue::ResolveLocal);
    QScriptValue h = obj.property(QStringLiteral("height"), QScriptValue::ResolveLocal);
    QScriptValue x = obj.property(QStringLiteral("x"), QScriptValue::ResolveLocal);
    QScriptValue y = obj.property(QStringLiteral("y"), QScriptValue::ResolveLocal);

    if (!w.isUndefined() && !h.isUndefined() && !x.isUndefined() && !y.isUndefined()) {
        rect.setX(x.toInt32());
        rect.setY(y.toInt32());
        rect.setWidth(w.toInt32());
        rect.setHeight(h.toInt32());
    }
}
// End of meta for QRect object

QScriptValue AbstractClient::toScriptValue(QScriptEngine *engine, const KAbstractClientRef &client)
{
    return engine->newQObject(client, QScriptEngine::QtOwnership,
                              QScriptEngine::ExcludeChildObjects |
                              QScriptEngine::ExcludeDeleteLater |
                              QScriptEngine::PreferExistingWrapperObject |
                              QScriptEngine::AutoCreateDynamicProperties);
}

void AbstractClient::fromScriptValue(const QScriptValue &value, KWin::AbstractClient *&client)
{
    client = qobject_cast<KWin::AbstractClient *>(value.toQObject());
}

QScriptValue X11Client::toScriptValue(QScriptEngine *eng, const KClientRef &client)
{
    return eng->newQObject(client, QScriptEngine::QtOwnership,
                           QScriptEngine::ExcludeChildObjects |
                           QScriptEngine::ExcludeDeleteLater |
                           QScriptEngine::PreferExistingWrapperObject |
                           QScriptEngine::AutoCreateDynamicProperties);
}

void X11Client::fromScriptValue(const QScriptValue &value, KWin::X11Client *&client)
{
    client = qobject_cast<KWin::X11Client *>(value.toQObject());
}

QScriptValue Toplevel::toScriptValue(QScriptEngine *eng, const KToplevelRef &client)
{
    return eng->newQObject(client, QScriptEngine::QtOwnership,
                           QScriptEngine::ExcludeChildObjects |
                           QScriptEngine::ExcludeDeleteLater |
                           QScriptEngine::PreferExistingWrapperObject |
                           QScriptEngine::AutoCreateDynamicProperties);
}

void Toplevel::fromScriptValue(const QScriptValue &value, KToplevelRef &client)
{
    client = qobject_cast<KWin::Toplevel*>(value.toQObject());
}

// Other helper functions
void KWin::MetaScripting::registration(QScriptEngine* eng)
{
    qScriptRegisterMetaType<QPoint>(eng, Point::toScriptValue, Point::fromScriptValue);
    qScriptRegisterMetaType<QSize>(eng, Size::toScriptValue, Size::fromScriptValue);
    qScriptRegisterMetaType<QRect>(eng, Rect::toScriptValue, Rect::fromScriptValue);
    qScriptRegisterMetaType<KAbstractClientRef>(eng, AbstractClient::toScriptValue, AbstractClient::fromScriptValue);
    qScriptRegisterMetaType<KClientRef>(eng, X11Client::toScriptValue, X11Client::fromScriptValue);
    qScriptRegisterMetaType<KToplevelRef>(eng, Toplevel::toScriptValue, Toplevel::fromScriptValue);

    qScriptRegisterSequenceMetaType<QStringList>(eng);
    qScriptRegisterSequenceMetaType< QList<KWin::AbstractClient*> >(eng);
    qScriptRegisterSequenceMetaType< QList<KWin::X11Client *> >(eng);
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
     */
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
            int len = (array.property(QStringLiteral("length")).isValid()) ? array.property(QStringLiteral("length")).toNumber() : 0;

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
    configObject.setProperty(QStringLiteral("get"), eng->newFunction(getConfigValue, 0), QScriptValue::Undeletable);
    configObject.setProperty(QStringLiteral("exists"), eng->newFunction(configExists, 0), QScriptValue::Undeletable);
    configObject.setProperty(QStringLiteral("loaded"), ((scriptConfig.toHash().empty()) ? eng->newVariant((bool)0) : eng->newVariant((bool)1)), QScriptValue::Undeletable);
    (eng->globalObject()).setProperty(QStringLiteral("config"), configObject);
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
