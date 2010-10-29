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

#ifndef __KWIN__SCRIPTING_CLIENT_H
#define __KWIN__SCRIPTING_CLIENT_H

#include <QDebug>
#include "./../workspace.h"
#include "./../client.h"

#include "toplevel.h"
#include "windowinfo.h"
#include "meta.h"

#include <QScriptEngine>
#include <QPair>

/**
  * BOOLEXPORTCLIENT and BOOLATTACHCLIENT are together used to export and connect
  * functions like client.isMaximizable, client.isShaded etc.
  */

#define BOOLEXPORTCLIENT(name) \
QScriptValue SWrapper::Client::name(QScriptContext* ctx, QScriptEngine* eng) { \
    KWin::Client* central = eng->fromScriptValue<KWin::Client*>(ctx->thisObject()); \
    \
    if(central == 0) { \
        return eng->undefinedValue(); \
    } else { \
        return eng->toScriptValue<bool>(central->name()); \
    } \
}

#define BOOLATTACHCLIENT(name) \
value.setProperty(#name, eng->newFunction(name, 0), QScriptValue::Undeletable);

namespace SWrapper
{

class Client;
/**
  * ClientResolution is a nice placeholder for a SWrapper::Client
  * and the corresponding QScriptValue that is generated.
  */
typedef QPair<SWrapper::Client*, QScriptValue> ClientResolution;

class Client : public Toplevel
    {
        Q_OBJECT

        //Q_PROPERTY(QString caption READ caption)
    private:
        //No longer using a vector for client-qsvalue mapping
        //static QVector<ClientMapper*> clientMap;
        //We're using QHash instead!!
        //We're not even using QHash anymore :)

        /** Generates a new wrapper and a corresponding QScriptValue
          * and returns it as a ClientResolution
          */
        static ClientResolution newWrapper(KWin::Client*, QScriptEngine*);

        /**
          * The KWin::Client* object that this wrapper handles
          * or 'wraps'.
          */
        KWin::Client* centralObject;

        /**
          * The QScriptEngine this wrapper is associated with
          */
        QScriptEngine* engine;

    private slots:
        /**
          * Emitted when the client is moved or resized
          */
        void sl_clientMoved();
        /**
          * Emitted when the client is maximized
          */
        void sl_maximizeSet(QPair<bool, bool>);

    signals:
        /**
          * Various events of the client.
          */
        void clientMoved();
        /**
          * The parameter is a two-element object in the format
          * {horizontally, vertically}
          */
        void maximizeSet(QScriptValue);
        void minimized();
        void gotFocus();
        void fullScreenSet(bool, bool);
        void unminimized();
        void restored();
        void onSetKeepAbove(bool);

    public:
        Client(KWin::Client*);

        //static bool clientRelease(const KWin::Client*);
        /**
          * Generate a new QScriptValue. This function can be directly used
          * by outside functions as it handles looking up the cache,
          * generation and validation of the cache
          */
        static QScriptValue generate(KWin::Client*, QScriptEngine*);

        /**
          * Sets the QScriptEngine for the current wrapper
          */
        void setEngine(QScriptEngine*);

        /**
          * Returns the KWin::Client* object this wrapper 'wraps'
          */
        KWin::Client* getCentralObject();

        // Forwaded functions: These functions emit signals, just like the sl_
        // signals, however, the client object is not available and hence
        // must be resolved. These are not slots and hence must be called i.e.
        // they cannot be connected to any signal.
        static void fw_clientActivated(KWin::Client*);

        /**
          * A variety of functions, all are self-explanatory.
          */

        static QScriptValue caption(QScriptContext*, QScriptEngine*);
        static QScriptValue close(QScriptContext*, QScriptEngine*);
        static QScriptValue resize(QScriptContext*, QScriptEngine*);
        static QScriptValue move(QScriptContext*, QScriptEngine*);
        static QScriptValue setGeometry(QScriptContext*, QScriptEngine*);
        static QScriptValue getWindowInfo(QScriptContext*, QScriptEngine*);
        static QScriptValue isTransient(QScriptContext*, QScriptEngine*);
        static QScriptValue transientFor(QScriptContext*, QScriptEngine*);
        static QScriptValue activate(QScriptContext*, QScriptEngine*);
        static QScriptValue setCaption(QScriptContext*, QScriptEngine*);
        static QScriptValue setFullScreen(QScriptContext*, QScriptEngine*);
        static QScriptValue isFullScreen(QScriptContext*, QScriptEngine*);
        static QScriptValue isFullScreenable(QScriptContext*, QScriptEngine*);
        static QScriptValue unminimize(QScriptContext*, QScriptEngine*);
        static QScriptValue minimize(QScriptContext*, QScriptEngine*);
        static QScriptValue clientGroup(QScriptContext*, QScriptEngine*);
        static QScriptValue setMaximize(QScriptContext*, QScriptEngine*);
        static QScriptValue maximize(QScriptContext*, QScriptEngine*);
        static QScriptValue desktop(QScriptContext*, QScriptEngine*);
        static QScriptValue keepAbove(QScriptContext*, QScriptEngine*);
        static QScriptValue keepBelow(QScriptContext*, QScriptEngine*);
        static QScriptValue setKeepAbove(QScriptContext*, QScriptEngine*);
        static QScriptValue setKeepBelow(QScriptContext*, QScriptEngine*);

        static QScriptValue isShade(QScriptContext*, QScriptEngine*);
        static QScriptValue isShadeable(QScriptContext*, QScriptEngine*);
        static QScriptValue isMinimized(QScriptContext*, QScriptEngine*);
        static QScriptValue isMinimizable(QScriptContext*, QScriptEngine*);
        static QScriptValue isMaximizable(QScriptContext*, QScriptEngine*);
        static QScriptValue isResizable(QScriptContext*, QScriptEngine*);
        static QScriptValue isMovable(QScriptContext*, QScriptEngine*);
        static QScriptValue isMovableAcrossScreens(QScriptContext*, QScriptEngine*);
        static QScriptValue isCloseable(QScriptContext*, QScriptEngine*);
        static QScriptValue isNormal(QScriptContext*, QScriptEngine*);

    };

}

#endif
