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

#include "s_clientgroup.h"
#include "meta.h"
#include "workspace.h"

Q_DECLARE_METATYPE(SWrapper::ClientGroup*)

SWrapper::ClientGroup::ClientGroup(KWin::ClientGroup* group)
{
    centralObject = group;

    if (group == 0) {
        invalid = true;
    } else {
        setParent(group);
        invalid = false;
    }
}

SWrapper::ClientGroup::ClientGroup(KWin::Client* client)
{
    if (client == 0) {
        invalid = true;
        centralObject = 0;
        return;
    }

    KWin::ClientGroup* cGrp = client->clientGroup();

    if (cGrp == 0) {
        cGrp = new KWin::ClientGroup(client);
    }

    centralObject = cGrp;
    setParent(cGrp);
    invalid = false;
}

KWin::ClientGroup* SWrapper::ClientGroup::getCentralObject()
{
    return centralObject;
}

/*
 * This was just to check how bindings were working..
 * Nothing to see here
 *
QScriptValue SWrapper::ClientGroup::toString(QScriptContext* ctx, QScriptEngine* eng) {
    SWrapper::ClientGroup* self = qscriptvalue_cast<SWrapper::ClientGroup*>(ctx->thisObject());
    qDebug()<<"Generic object at ["<<(void*)self<<"]";

    if (self != 0) {
    qDebug()<<"    Wrapping: ["<<(void*)(self->centralObject)<<"] (i: "<<self->invalid<<")";
    }

    return QScriptValue(eng, QString("."));
}
*/

QScriptValue SWrapper::ClientGroup::add(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* client = qobject_cast<KWin::Client*>(ctx->argument(0).toQObject());
    KWin::ClientGroup* cGrp = qscriptvalue_cast<KWin::ClientGroup*>(ctx->thisObject());

    if ((client == 0) || (cGrp == 0)) {
        return QScriptValue(eng, (bool)0);
    } else {
        int before = (ctx->argument(1)).isUndefined() ? (-1) : ((ctx->argument(0)).toNumber());
        bool becomeVisible = (ctx->argument(2)).isUndefined() ? (false) : ((ctx->argument(0)).toBool());

        if (client->clientGroup()) {
            (client->clientGroup())->remove(client);
        }

        cGrp->add(client, before, becomeVisible);
        return QScriptValue(eng, (bool)1);
    }
}

QScriptValue SWrapper::ClientGroup::remove(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::ClientGroup* cGrp = qscriptvalue_cast<KWin::ClientGroup*>(ctx->thisObject());
    QScriptValue arg = ctx->argument(0);
    QRect geom = eng->fromScriptValue<QRect>(ctx->argument(1));

    if (cGrp == 0) {
        return eng->toScriptValue<bool>(0);
    }

    if (arg.isNumber()) {
        cGrp->remove(arg.toNumber(), geom, false);
        return eng->toScriptValue<bool>(1);
    } else {
        KWin::Client* client = qobject_cast<KWin::Client*>(arg.toQObject());

        if (client == 0) {
            return eng->toScriptValue<bool>(0);
        } else {
            cGrp->remove(client, geom, false);
            return eng->toScriptValue<bool>(1);
        }
    }
}

QScriptValue SWrapper::ClientGroup::contains(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::ClientGroup* cGrp = qscriptvalue_cast<KWin::ClientGroup*>(ctx->thisObject());

    if (cGrp == 0) {
        return QScriptValue();
    } else {
        KWin::Client* client = qobject_cast<KWin::Client*>(ctx->argument(0).toQObject());

        if (client == 0) {
            return QScriptValue();
        } else {
            return eng->toScriptValue<bool>(cGrp->contains(client));
        }
    }
}

QScriptValue SWrapper::ClientGroup::indexOf(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::ClientGroup* cGrp = qscriptvalue_cast<KWin::ClientGroup*>(ctx->thisObject());

    if (cGrp == 0) {
        return QScriptValue();
    } else {
        KWin::Client* client = qobject_cast<KWin::Client*>(ctx->argument(0).toQObject());

        if (client == 0) {
            return QScriptValue();
        } else {
            return eng->toScriptValue<int>(cGrp->indexOfClient(client));
        }
    }
}

QScriptValue SWrapper::ClientGroup::move(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::ClientGroup* cGrp = qscriptvalue_cast<KWin::ClientGroup*>(ctx->thisObject());

    // This is one weird function. It can be called like:
    // move(client, client) OR move(client, index) OR move(index, client)
    // move(index, index)
    // NOTE: other than move(client, client), all other calls are mapped to
    // move(index, index) using indexof(client)

    QScriptValue arg1 = ctx->argument(0);
    QScriptValue arg2 = ctx->argument(1);

    KWin::Client* cl1 = qobject_cast<KWin::Client*>(arg1.toQObject());
    KWin::Client* cl2 = qobject_cast<KWin::Client*>(arg2.toQObject());

    if (cl1 != 0) {
        if (cl2 == 0) {
            if (!(arg2.isNumber())) {
                return eng->toScriptValue<bool>(0);
            } else {
                cGrp->move(cGrp->indexOfClient(cl1), (int)arg2.toNumber());
                return eng->toScriptValue<bool>(1);
            }
        } else {
            cGrp->move(cl1, cl2);
            return eng->toScriptValue<bool>(1);
        }
    } else {
        if (!(arg1.isNumber())) {
            return eng->toScriptValue<bool>(0);
        } else {
            if (cl2 != 0) {
                cGrp->move((int)arg1.toNumber(), cGrp->indexOfClient(cl2));
                return eng->toScriptValue<bool>(1);
            } else {
                if (!arg2.isNumber()) {
                    return eng->toScriptValue<bool>(0);
                } else {
                    cGrp->move((int)arg1.toNumber(), (int)arg2.toNumber());
                    return eng->toScriptValue<bool>(0);
                }
            }
        }
    }
}

QScriptValue SWrapper::ClientGroup::removeAll(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::ClientGroup* cGrp = qscriptvalue_cast<KWin::ClientGroup*>(ctx->thisObject());

    if (cGrp == 0) {
        return eng->toScriptValue<bool>(0);
    } else {
        cGrp->removeAll();
        return eng->toScriptValue<bool>(1);
    }
}

QScriptValue SWrapper::ClientGroup::closeAll(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::ClientGroup* cGrp = qscriptvalue_cast<KWin::ClientGroup*>(ctx->thisObject());

    if (cGrp == 0) {
        return eng->toScriptValue<bool>(0);
    } else {
        cGrp->closeAll();
        return eng->toScriptValue<bool>(1);
    }
}

QScriptValue SWrapper::ClientGroup::minSize(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::ClientGroup* cGrp = qscriptvalue_cast<KWin::ClientGroup*>(ctx->thisObject());

    if (cGrp == 0) {
        return QScriptValue();
    } else {
        return eng->toScriptValue<QSize>(cGrp->minSize());
    }
}

QScriptValue SWrapper::ClientGroup::maxSize(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::ClientGroup* cGrp = qscriptvalue_cast<KWin::ClientGroup*>(ctx->thisObject());

    if (cGrp == 0) {
        return QScriptValue();
    } else {
        return eng->toScriptValue<QSize>(cGrp->maxSize());
    }
}

QScriptValue SWrapper::ClientGroup::clients(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::ClientGroup* cGrp = qscriptvalue_cast<KWin::ClientGroup*>(ctx->thisObject());

    if (cGrp == 0) {
        return QScriptValue();
    } else {
        KWin::ClientList clients = cGrp->clients();
        QScriptValue value = eng->newArray(clients.size());
        for (int i=0; i<clients.size(); ++i) {
            value.setProperty(i, SWrapper::valueForClient(clients.at(i), eng));
        }
        return value;
    }
}

QScriptValue SWrapper::ClientGroup::visible(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::ClientGroup* cGrp = qscriptvalue_cast<KWin::ClientGroup*>(ctx->thisObject());

    if (cGrp == 0) {
        return QScriptValue();
    } else {
        return valueForClient(cGrp->visible(), eng);
    }
}

QScriptValue SWrapper::ClientGroup::generate(QScriptEngine* eng, SWrapper::ClientGroup* cGroup)
{
    QScriptValue temp = eng->newQObject(cGroup, QScriptEngine::AutoOwnership);

    temp.setProperty("add", eng->newFunction(add, 3), QScriptValue::Undeletable);
    temp.setProperty("remove", eng->newFunction(remove, 1), QScriptValue::Undeletable);
    temp.setProperty("clients", eng->newFunction(clients, 0), QScriptValue::Undeletable);
    temp.setProperty("contains", eng->newFunction(contains, 1), QScriptValue::Undeletable);
    temp.setProperty("indexOf", eng->newFunction(indexOf, 1), QScriptValue::Undeletable);
    temp.setProperty("move", eng->newFunction(move, 2), QScriptValue::Undeletable);
    temp.setProperty("removeAll", eng->newFunction(removeAll, 0), QScriptValue::Undeletable);
    temp.setProperty("closeAll", eng->newFunction(closeAll, 0), QScriptValue::Undeletable);
    temp.setProperty("minSize", eng->newFunction(minSize, 0), QScriptValue::Undeletable);
    temp.setProperty("maxSize", eng->newFunction(maxSize, 0), QScriptValue::Undeletable);
    temp.setProperty("visible", eng->newFunction(visible, 0), QScriptValue::Undeletable);
    return temp;
}

QScriptValue SWrapper::ClientGroup::construct(QScriptContext* ctx, QScriptEngine* eng)
{
    return generate(eng, new SWrapper::ClientGroup(
                        qobject_cast<KWin::Client*>(ctx->argument(0).toQObject())
                    ));
}

QScriptValue SWrapper::ClientGroup::publishClientGroupClass(QScriptEngine* eng)
{
    QScriptValue proto = generate(eng, new SWrapper::ClientGroup((KWin::ClientGroup*)0));
    eng->setDefaultPrototype(qMetaTypeId<SWrapper::ClientGroup*>(), proto);

    return eng->newFunction(construct, proto);
}
