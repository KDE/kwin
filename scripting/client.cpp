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

#include "client.h"

SWrapper::Client::Client(KWin::Client* client) :
    Toplevel(client)
{
    if (client != 0) {
        QObject::connect(client, SIGNAL(s_clientMoved()), this, SLOT(sl_clientMoved()));
        QObject::connect(client, SIGNAL(maximizeSet(QPair<bool,bool>)), this, SLOT(sl_maximizeSet(QPair<bool,bool>)));
        QObject::connect(client, SIGNAL(s_minimized()), this, SIGNAL(minimized()));
        QObject::connect(client, SIGNAL(s_activated()), this, SIGNAL(gotFocus()));
        QObject::connect(client, SIGNAL(s_fullScreenSet(bool,bool)), this, SIGNAL(fullScreenSet(bool,bool)));
        QObject::connect(client, SIGNAL(s_setKeepAbove(bool)), this, SIGNAL(onSetKeepAbove(bool)));
        QObject::connect(client, SIGNAL(s_unminimized()), this, SIGNAL(unminimized()));
        QObject::connect(this, SIGNAL(unminimized()), this, SIGNAL(restored()));
    }

    centralObject = client;
}

KWin::Client* SWrapper::Client::getCentralObject()
{
    return centralObject;
}

/*
SWrapper::ClientResolution SWrapper::Client::clientResolve(const QScriptEngine*, const KWin::Client* client) {
    if (client == 0) {
        return ClientResolution(0, QScriptValue());
    } else {
        return
    }
}
*/

void SWrapper::Client::setEngine(QScriptEngine* eng)
{
    engine = eng;
}

void SWrapper::Client::sl_clientMoved()
{
    emit clientMoved();
}

void SWrapper::Client::sl_maximizeSet(QPair<bool, bool> param)
{
    QScriptValue temp = engine->newObject();
    temp.setProperty("v", engine->toScriptValue(param.first));
    temp.setProperty("h", engine->toScriptValue(param.second));

    emit maximizeSet(temp);
}

/*
bool SWrapper::Client::clientRelease(const KWin::Client* client) {
    if (client == 0) {
    return false;
    } else {
    if (client->swrapper != 0) {
        delete client->swrapper;
        return true;
    }
    }

    return false;
}
*/

//newWrapper does not search clientMap for existing entries
//a search must be performed prior to this call.
//However, since now clientMap is actually a QHash, it wouldn't
//hurt to perform a check here as well.
SWrapper::ClientResolution SWrapper::Client::newWrapper(KWin::Client* client, QScriptEngine* eng)
{
    SWrapper::Client* wrapper = new SWrapper::Client(client);
    wrapper->setEngine(eng);
    QScriptValue value;

    // Use this value to avoid repeated function generations
    // for function aliases.
    QScriptValue func;

    /*
     * Step1: Prepping
     * Connect all signals and slots from client to
     * the wrapper object.
     */


    //End of prepping

    /*
     * Step2: Conversion
     * Wrap the object, set the data parameter
     * and obtain a QScriptValue
     */

    value = eng->newQObject(wrapper,
                            QScriptEngine::AutoOwnership,
                            QScriptEngine::ExcludeSuperClassContents | QScriptEngine::ExcludeDeleteLater
                           );

    wrapper->tl_centralObject = client;
    tl_append(value, eng);

    //End of Conversion

    /*
     * Step3: Spice
     * Add the static functions and other things
     * that rely on the value of data() and not the
     * wrapper address.
     */

    value.setProperty("caption", eng->newFunction(caption, 0), QScriptValue::Undeletable);
    value.setProperty("close", eng->newFunction(close, 0), QScriptValue::Undeletable);
    value.setProperty("resize", eng->newFunction(resize, 1), QScriptValue::Undeletable);
    value.setProperty("move", eng->newFunction(move, 1), QScriptValue::Undeletable);
    value.setProperty("setGeometry", eng->newFunction(setGeometry, 1), QScriptValue::Undeletable);
    value.setProperty("getWindowInfo", eng->newFunction(getWindowInfo, 0), QScriptValue::Undeletable);
    value.setProperty("isTransient", eng->newFunction(isTransient, 0), QScriptValue::Undeletable);
    value.setProperty("transientFor", eng->newFunction(transientFor, 0), QScriptValue::Undeletable);
    value.setProperty("activate", eng->newFunction(activate, 1), QScriptValue::Undeletable);
    value.setProperty("setCaption", eng->newFunction(setCaption, 1), QScriptValue::Undeletable);
    value.setProperty("setFullScreen", eng->newFunction(setFullScreen, 2), QScriptValue::Undeletable);
    value.setProperty("isFullScreen", eng->newFunction(isFullScreen, 0), QScriptValue::Undeletable);
    value.setProperty("isFullScreenable", eng->newFunction(isFullScreenable, 0), QScriptValue::Undeletable);
    value.setProperty("minimize", eng->newFunction(minimize, 0), QScriptValue::Undeletable);
    value.setProperty("clientGroup", eng->newFunction(clientGroup, 0), QScriptValue::Undeletable);
    value.setProperty("maximize", eng->newFunction(maximize, 0), QScriptValue::Undeletable);
    value.setProperty("setMaximize", eng->newFunction(setMaximize, 2), QScriptValue::Undeletable);
    value.setProperty("desktop", eng->newFunction(desktop, 0), QScriptValue::Undeletable);
    value.setProperty("setKeepAbove", eng->newFunction(setKeepAbove, 0), QScriptValue::Undeletable);
    value.setProperty("setKeepBelow", eng->newFunction(setKeepBelow, 0), QScriptValue::Undeletable);

    BOOLATTACHCLIENT(isShade)
    BOOLATTACHCLIENT(isShadeable)
    BOOLATTACHCLIENT(isMinimized)
    BOOLATTACHCLIENT(isMinimizable)
    BOOLATTACHCLIENT(isMaximizable)
    BOOLATTACHCLIENT(isResizable)
    BOOLATTACHCLIENT(isMovable)
    BOOLATTACHCLIENT(isMovableAcrossScreens)
    BOOLATTACHCLIENT(isCloseable)
    BOOLATTACHCLIENT(keepAbove)
    BOOLATTACHCLIENT(keepBelow)

    func = eng->newFunction(unminimize, 0);
    value.setProperty("unminimize", func, QScriptValue::Undeletable);
    value.setProperty("restore", func, QScriptValue::Undeletable);

    ClientResolution final(wrapper, value);

    //Finally, append the entire thing to the clientMap
    //clientMap.insert(const_cast<KWin::Client*>(client), ClientResolution(wrapper, value));

    //And.. we're done
    (client->scriptCache)->insert(eng, final);
    return final;
}

QScriptValue SWrapper::Client::generate(KWin::Client* client, QScriptEngine* eng)
{
    if (client == 0) {
        return QScriptValue();
    } else {
        ClientResolution res = (client->scriptCache)->value(eng, ClientResolution(0, QScriptValue()));

        if (res.first != 0) {
            //The required object has been already created
            //Return that instead of creating a new one
            return res.second;
        } else {
            res = newWrapper(client, eng);
            return res.second;
        }
    }
}

QScriptValue SWrapper::Client::setMaximize(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());

    if (central == 0) {
        return eng->toScriptValue<bool>(0);
    } else {
        central->setMaximize((ctx->argument(0)).toBool(), (ctx->argument(1)).toBool());
        return eng->toScriptValue<bool>(1);
    }
}

BOOLEXPORTCLIENT(isShade)
BOOLEXPORTCLIENT(isShadeable)
BOOLEXPORTCLIENT(isMinimized)
BOOLEXPORTCLIENT(isMinimizable)
BOOLEXPORTCLIENT(isMaximizable)
BOOLEXPORTCLIENT(isResizable)
BOOLEXPORTCLIENT(isMovable)
BOOLEXPORTCLIENT(isMovableAcrossScreens)
BOOLEXPORTCLIENT(isCloseable)
BOOLEXPORTCLIENT(keepAbove)
BOOLEXPORTCLIENT(keepBelow)

QScriptValue SWrapper::Client::setKeepAbove(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());
    QScriptValue setValue = ctx->argument(0);

    if ((central == 0) || (setValue.isUndefined())) {
        return eng->toScriptValue<bool>(0);
    } else {
        central->setKeepAbove(eng->fromScriptValue<bool>(setValue));
        return eng->toScriptValue<bool>(1);
    }
}

QScriptValue SWrapper::Client::setKeepBelow(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());
    QScriptValue setValue = ctx->argument(0);

    if ((central == 0) || (setValue.isUndefined())) {
        return eng->toScriptValue<bool>(0);
    } else {
        central->setKeepBelow(eng->fromScriptValue<bool>(setValue));
        return eng->toScriptValue<bool>(1);
    }
}

QScriptValue SWrapper::Client::isNormal(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());

    if (central == 0) {
        return eng->undefinedValue();
    } else {
        return eng->toScriptValue<bool>(!central->isSpecialWindow());
    }
}

QScriptValue SWrapper::Client::maximize(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());

    if (central == 0) {
        return eng->toScriptValue<bool>(0);
    } else {
        central->setMaximize(1, 1);
        return eng->toScriptValue<bool>(1);
    }
}

QScriptValue SWrapper::Client::desktop(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());

    if (central == 0) {
        return eng->undefinedValue();
    } else {
        return eng->toScriptValue(central->desktop());
    }
}

QScriptValue SWrapper::Client::clientGroup(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());

    if (central == 0) {
        return QScriptValue();
    } else {
        return eng->toScriptValue<KWin::ClientGroup*>(central->clientGroup());
    }
}

QScriptValue SWrapper::Client::caption(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());

    if (central == 0) {
        return QScriptValue();
    } else {
        return eng->toScriptValue(central->caption());
    }
}

QScriptValue SWrapper::Client::setCaption(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());

    if (ctx->argument(0).isUndefined()) {
        return eng->toScriptValue<bool>(0);
    } else {
        QString cap = (ctx->argument(0)).toString();
        central->setCaption(cap);
        return eng->toScriptValue<bool>(1);
    }
}

QScriptValue SWrapper::Client::resize(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());
    QScriptValue arg = ctx->argument(0);
    QSize size;
    bool emitJs;
    QScriptValue emitJsQs;

    if (central == 0 || arg.isUndefined()) {
        return eng->toScriptValue<bool>(0);
    } else {
        if (arg.isNumber()) {
            size = QSize(arg.toInt32(), (ctx->argument(1)).toInt32());
            emitJsQs = ctx->argument(2);
        } else if (arg.isObject()) {
            size = eng->fromScriptValue<QSize>(arg);
            emitJsQs = ctx->argument(1);
        }

        if (emitJsQs.isUndefined() || !emitJsQs.isValid()) {
            emitJs = true;
        } else {
            emitJs = emitJsQs.toBool();
        }

        if (size.isValid()) {
            central->plainResize(size, KWin::NormalGeometrySet, emitJs);
            return eng->toScriptValue<bool>(0);
        } else {
            return eng->toScriptValue<bool>(1);
        }
    }
}

QScriptValue SWrapper::Client::move(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());
    QScriptValue arg = ctx->argument(0);

    if (central == 0 || arg.isUndefined()) {
        return eng->toScriptValue<bool>(0);
    } else {
        QPoint point;
        bool emitJs;
        QScriptValue emitJsQs;

        if (arg.isNumber()) {
            point = QPoint(arg.toInt32(), (ctx->argument(1)).toInt32());
            emitJsQs = ctx->argument(2);
        } else if (arg.isObject()) {
            point = eng->fromScriptValue<QPoint>(arg);
            emitJsQs = ctx->argument(1);
        }

        if (emitJsQs.isUndefined() || !emitJsQs.isValid()) {
            emitJs = true;
        } else {
            emitJs = emitJsQs.toBool();
        }


        central->setGeometry(QRect(point, central->size()), KWin::NormalGeometrySet, emitJs);
        return eng->toScriptValue<bool>(1);
    }
}

QScriptValue SWrapper::Client::setGeometry(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());
    QScriptValue arg = ctx->argument(0);
    QRect rect;
    bool emitJs;
    QScriptValue emitJsQs;

    if (central == 0 || arg.isUndefined()) {
        return eng->toScriptValue<bool>(0);
    } else {
        if (arg.isNumber()) {
            rect = QRect(arg.toInt32(), (ctx->argument(1)).toInt32(),
                         (ctx->argument(2)).toInt32(), (ctx->argument(3)).toInt32());
            emitJsQs = ctx->argument(4);
        } else if (arg.isObject()) {
            rect = eng->fromScriptValue<QRect>(arg);
            emitJsQs = ctx->argument(1);
        }

        if (emitJsQs.isUndefined() || !emitJsQs.isValid()) {
            emitJs = true;
        } else {
            emitJs = emitJsQs.toBool();
        }

        central->setGeometry(rect, KWin::NormalGeometrySet, emitJs);
        return eng->toScriptValue<bool>(1);
    }
}

QScriptValue SWrapper::Client::getWindowInfo(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());

    if (central == 0) {
        return QScriptValue();
    } else {
        // For now at least, just get all properties. Tweaking will be taken care
        // of later. TODO
        KWindowInfo info = KWindowSystem::windowInfo(central->window(), -1U, -1U);
        return SWrapper::WindowInfo::generate(info, eng, central);
    }
}

/*
QString SWrapper::Client::caption() {
    return centralObject->caption();
}*/

QScriptValue SWrapper::Client::unminimize(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());

    if (central == 0) {
        return eng->toScriptValue<bool>(0);
    } else {
        central->unminimize(false);
        return eng->toScriptValue<bool>(1);
    }
}

QScriptValue SWrapper::Client::minimize(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());

    if (central == 0) {
        return eng->toScriptValue<bool>(0);
    } else {
        central->minimize(false);
        return eng->toScriptValue<bool>(1);
    }
}

QScriptValue SWrapper::Client::close(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());

    if (central == 0) {
        return QScriptValue();
    } else {
        central->closeWindow();
        return eng->toScriptValue(true);
    }
}

QScriptValue SWrapper::Client::isTransient(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());

    if (central == 0) {
        return QScriptValue();
    } else {
        bool x = central->isTransient();
        return eng->toScriptValue(x);
    }
}

QScriptValue SWrapper::Client::transientFor(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());

    if (central == 0) {
        return QScriptValue();
    } else {
        return eng->toScriptValue(central->transientFor());
    }
}

QScriptValue SWrapper::Client::setFullScreen(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());
    QScriptValue set = ctx->argument(0);
    QScriptValue user = ctx->argument(0);

    if (set.isUndefined() || user.isUndefined()) {
        return QScriptValue();
    }

    if (central == 0) {
        return eng->toScriptValue<bool>(0);
    } else {
        central->setFullScreen(set.toBool(), user.toBool());
        return eng->toScriptValue<bool>(1);
    }
}

QScriptValue SWrapper::Client::isFullScreen(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());

    if (central == 0) {
        return QScriptValue();
    } else {
        return eng->toScriptValue<bool>(central->isFullScreen());
    }
}

QScriptValue SWrapper::Client::isFullScreenable(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());

    if (central == 0) {
        return QScriptValue();
    } else {
        return eng->toScriptValue<bool>(central->isFullScreenable());
    }
}

QScriptValue SWrapper::Client::activate(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject());
    QScriptValue force = ctx->argument(0);
    bool _force = 0;

    if (central == 0) {
        return eng->toScriptValue<bool>(0);
    } else {
        if (!force.isBool()) {
            _force = force.toBool();
        }

        (KWin::Workspace::self())->activateClient(central, _force);
        return eng->toScriptValue<bool>(1);
    }
}
