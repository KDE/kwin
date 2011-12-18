/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Rohan Prabhu <rohan@rohanprabhu.com>
Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>

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

#include "workspace.h"
#include "meta.h"

KWin::Workspace*  SWrapper::Workspace::centralObject = 0;

QScriptValue SWrapper::valueForClient(KWin::Client *client, QScriptEngine *engine) {
    return engine->newQObject(client);
}

SWrapper::Workspace::Workspace(QObject* parent) : QObject(parent)
{
    if (centralObject == 0) {
        return;
    } else {
        SWrapper::WorkspaceProxy* proxy = SWrapper::WorkspaceProxy::instance();

        QObject::connect(centralObject, SIGNAL(desktopPresenceChanged(KWin::Client*,int)),
                         this, SLOT(sl_desktopPresenceChanged(KWin::Client*,int))
                        );

        QObject::connect(centralObject, SIGNAL(currentDesktopChanged(int)),
                         this, SLOT(sl_currentDesktopChanged(int))
                        );

        QObject::connect(centralObject, SIGNAL(clientAdded(KWin::Client*)),
                         this, SLOT(sl_clientAdded(KWin::Client*))
                        );

        QObject::connect(centralObject, SIGNAL(clientRemoved(KWin::Client*)),
                         this, SLOT(sl_clientRemoved(KWin::Client*))
                        );

        QObject::connect(centralObject, SIGNAL(clientActivated(KWin::Client*)),
                         this, SLOT(sl_clientActivated(KWin::Client*))
                        );

        QObject::connect(centralObject, SIGNAL(groupAdded(KWin::Group*)),
                         this, SLOT(sl_groupAdded(KWin::Group*))
                        );

        QObject::connect(proxy, SIGNAL(clientMinimized(KWin::Client*)),
                         this, SLOT(sl_clientMinimized(KWin::Client*))
                        );

        QObject::connect(proxy, SIGNAL(clientUnminimized(KWin::Client*)),
                         this, SLOT(sl_clientUnminimized(KWin::Client*))
                        );

        QObject::connect(proxy, SIGNAL(clientMaximizeSet(KWin::Client*,QPair<bool,bool>)),
                         this, SLOT(sl_clientMaximizeSet(KWin::Client*,QPair<bool,bool>))
                        );

        QObject::connect(proxy, SIGNAL(clientManaging(KWin::Client*)),
                         this, SLOT(sl_clientManaging(KWin::Client*))
                        );

        QObject::connect(proxy, SIGNAL(killWindowCalled(KWin::Client*)),
                         this, SLOT(sl_killWindowCalled(KWin::Client*))
                        );

        QObject::connect(proxy, SIGNAL(clientFullScreenSet(KWin::Client*,bool,bool)),
                         this, SLOT(sl_clientFullScreenSet(KWin::Client*,bool,bool))
                        );

        QObject::connect(proxy, SIGNAL(clientSetKeepAbove(KWin::Client*,bool)),
                         this, SLOT(sl_clientSetKeepAbove(KWin::Client*,bool))
                        );


    }
}

void SWrapper::Workspace::sl_desktopPresenceChanged(KWin::Client* client, int prev_desk)
{
    if (centralEngine == 0) {
        return;
    } else {
        emit desktopPresenceChanged(valueForClient(client, centralEngine),
                                    centralEngine->toScriptValue(prev_desk)
                                   );
    }
}

void SWrapper::Workspace::sl_clientAdded(KWin::Client* client)
{
    if (centralEngine == 0) {
        return;
    } else {
        emit clientAdded(valueForClient(client, centralEngine));
    }
}

void SWrapper::Workspace::sl_clientFullScreenSet(KWin::Client* client, bool set, bool user)
{
    if (centralEngine == 0) {
        return;
    } else {
        emit clientFullScreenSet(valueForClient(client, centralEngine),
                                 centralEngine->toScriptValue<bool>(set),
                                 centralEngine->toScriptValue<bool>(user)
                                );
    }
}

void SWrapper::Workspace::sl_clientSetKeepAbove(KWin::Client* client, bool set)
{
    if (centralEngine == 0) {
        return;
    } else {
        emit clientSetKeepAbove(valueForClient(client, centralEngine),
                                centralEngine->toScriptValue<bool>(set)
                               );
    }
}

void SWrapper::Workspace::sl_currentDesktopChanged(int prev_desk)
{
    if (centralEngine == 0) {
        return;
    } else {
        emit currentDesktopChanged(centralEngine->toScriptValue(prev_desk));
    }
}

void SWrapper::Workspace::sl_clientRemoved(KWin::Client* client)
{
    if (centralEngine == 0) {
        return;
    } else {
        emit clientRemoved(valueForClient(client, centralEngine));
    }
}

void SWrapper::Workspace::sl_clientManaging(KWin::Client* client)
{
    if (centralEngine == 0) {
        return;
    } else {
        emit clientManaging(valueForClient(client, centralEngine));
    }
}

void SWrapper::Workspace::sl_clientMinimized(KWin::Client* client)
{
    if (centralEngine == 0) {
        return;
    } else {
        emit clientMinimized(valueForClient(client, centralEngine));
    }
}

void SWrapper::Workspace::sl_clientUnminimized(KWin::Client* client)
{
    if (centralEngine == 0) {
        return;
    } else {
        emit clientUnminimized(valueForClient(client, centralEngine));
        emit clientRestored(valueForClient(client, centralEngine));
    }
}

void SWrapper::Workspace::sl_clientMaximizeSet(KWin::Client* client, QPair<bool, bool> param)
{
    if (centralEngine == 0) {
        return;
    } else {
        QScriptValue temp = centralEngine->newObject();
        temp.setProperty("v", centralEngine->toScriptValue(param.first));
        temp.setProperty("h", centralEngine->toScriptValue(param.second));
        emit clientMaximizeSet(valueForClient(client, centralEngine), temp);
    }
}

void SWrapper::Workspace::sl_killWindowCalled(KWin::Client* client)
{
    if (centralEngine == 0) {
        return;
    } else {
        emit killWindowCalled(valueForClient(client, centralEngine));
    }
}

void SWrapper::Workspace::sl_clientActivated(KWin::Client* client)
{
    if (centralEngine == 0) {
        return;
    } else {
        emit clientActivated(valueForClient(client, centralEngine));
    }
}

void SWrapper::Workspace::sl_groupAdded(KWin::Group* group)
{
    Q_UNUSED(group);
    //TODO
}

bool SWrapper::Workspace::initialize(KWin::Workspace* wspace)
{
    if (wspace == 0) {
        return false;
    } else {
        SWrapper::Workspace::centralObject = wspace;
        return true;
    }
}

QScriptValue SWrapper::Workspace::getAllClients(QScriptContext* ctx, QScriptEngine* eng)
{
    const KWin::ClientList x = centralObject->stackingOrder();

    if (ctx->argumentCount() == 0) {
        QScriptValue array = eng->newArray(x.size());
        for (int i=0; i<x.size(); ++i) {
            array.setProperty(i, valueForClient(x.at(i), eng));
        }
        return array;
    } else {
        KWin::ClientList ret;
        int desk_no = (ctx->argument(0)).toNumber();
        if ((desk_no >= 1) && (desk_no > SWrapper::Workspace::centralObject->numberOfDesktops())) {
            return QScriptValue();
        } else {
            QScriptValue array = eng->newArray();
            for (int i = 0; i < x.size(); i++) {
                if (x.at(i)->desktop() == desk_no) {
                    ret.append(x.at(i));
                }
            }
            for (int i=0; i<ret.size(); ++i) {
                array.setProperty(i, valueForClient(ret.at(i), eng));
            }

            return array;
        }
    }
}

QScriptValue SWrapper::Workspace::activeClient(QScriptContext* ctx, QScriptEngine* eng)
{
    Q_UNUSED(ctx);
    return valueForClient(centralObject->activeClient(), eng);
}

QScriptValue SWrapper::Workspace::setCurrentDesktop(QScriptContext* ctx, QScriptEngine* /*eng*/)
{
    if (ctx->argumentCount() >= 1) {
        int num = ((ctx->argument(0)).isNumber()) ? ((ctx->argument(0)).toNumber()) : (-1);
        if (num != -1) {
            centralObject->setCurrentDesktop(num);
        }
    }

    return QScriptValue();
}

QScriptValue SWrapper::Workspace::dimensions(QScriptContext* ctx, QScriptEngine* eng)
{
    Q_UNUSED(ctx);
    return eng->toScriptValue(QSize(centralObject->workspaceWidth(), centralObject->workspaceHeight()));
}

QScriptValue SWrapper::Workspace::desktopGridSize(QScriptContext* ctx, QScriptEngine* eng)
{
    Q_UNUSED(ctx);
    return eng->toScriptValue(centralObject->desktopGridSize());
}

QScriptValue SWrapper::Workspace::clientGroups(QScriptContext* ctx, QScriptEngine* eng)
{
    Q_UNUSED(ctx);
    return eng->toScriptValue<QList<KWin::ClientGroup*> >(centralObject->clientGroups);
}

int SWrapper::Workspace::currentDesktop() const
{
    return centralObject->currentDesktop();
}

void SWrapper::Workspace::attach(QScriptEngine* engine)
{
    QScriptValue func;
    centralEngine = engine;

    QScriptValue self = engine->newQObject(
                            this,
                            QScriptEngine::QtOwnership,
                            QScriptEngine::ExcludeSuperClassContents | QScriptEngine::ExcludeDeleteLater
                        );

    func = engine->newFunction(setCurrentDesktop, 1);
    self.setProperty("setCurrentDesktop", func, QScriptValue::Undeletable);

    func = engine->newFunction(getAllClients, 0);
    self.setProperty("getAllClients", func, QScriptValue::Undeletable);

    func = engine->newFunction(dimensions, 0);
    self.setProperty("dimensions", func, QScriptValue::Undeletable);

    func = engine->newFunction(desktopGridSize, 0);
    self.setProperty("desktopGridSize", func, QScriptValue::Undeletable);
    self.setProperty("activeClient", engine->newFunction(activeClient, 0), QScriptValue::Undeletable);
    self.setProperty("clientGroups", engine->newFunction(clientGroups, 0), QScriptValue::Undeletable);

    (engine->globalObject()).setProperty("workspace", self, QScriptValue::Undeletable);
}
