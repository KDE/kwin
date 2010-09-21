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

#include "chelate.h"
#include "meta.h"

QList<KWin::Chelate::ChelateProxy*> KWin::Chelate::ChelateProxy::chMap;

/**
  * ChelateProxy | functions
  */

KWin::Chelate::ChelateProxy::ChelateProxy()
    {
    engine = 0;
    }

void KWin::Chelate::ChelateProxy::setEngine(QScriptEngine* _engine)
    {
    engine = _engine;
    }

void KWin::Chelate::ChelateProxy::setConnectors(QScriptValueList _connectors)
    {
    connectors = _connectors;
    }

void KWin::Chelate::ChelateProxy::setAction(QScriptValue _action)
    {
    action = _action;
    }

void KWin::Chelate::ChelateProxy::setEventStrings(QStringList _on)
    {
    on = _on;
    }

void KWin::Chelate::ChelateProxy::setFilter(QScriptValue _filter)
    {
    filter = _filter;
    }

void KWin::Chelate::ChelateProxy::setId(int _id)
    {
    id = _id;
    }

QScriptValue KWin::Chelate::ChelateProxy::getAction()
    {
    return action;
    }

QScriptValue KWin::Chelate::ChelateProxy::getFilter()
    {
    return filter;
    }

void KWin::Chelate::ChelateProxy::process(int index)
    {
    if(engine == 0)
        {
        return;
        }

    QScriptValue connector = connectors.at(index).property("connect");
    QScriptValue _scanner = engine->newFunction(KWin::Chelate::scanner, 0);
    _scanner.setData(engine->fromScriptValue<int>(id));
    QScriptValueList args;

    args<<_scanner;
    connector.call(connectors.at(index), args);
    }

void KWin::Chelate::ChelateProxy::processAll()
    {
    for(int i = 0; i<connectors.size(); i++)
        {
        process(i);
        }
    }
/** */
// END of ChelateProxy | functions
/** */

/**
  * KWin::Chelate | functions
  */

QScriptValue KWin::Chelate::scanner(QScriptContext* ctx, QScriptEngine* eng)
    {
    QScriptValue index = (ctx->callee()).data();
    KWin::Chelate::ChelateProxy* proxy = KWin::Chelate::ChelateProxy::chMap.at(index.toNumber());
    QScriptValue filter = proxy->getFilter();
    bool ret = (filter.call(QScriptValue(), ctx->argumentsObject())).toBool();

    if(ret == true)
        {
        QScriptValue action = proxy->getAction();
        action.call(QScriptValue(), ctx->argumentsObject());
        }

    return eng->undefinedValue();
    }

QScriptValue KWin::Chelate::rule(QScriptContext* ctx, QScriptEngine* eng)
    {
    ChelateProxy* proxy = new ChelateProxy();
    QScriptValue chRule = eng->newObject();
    QScriptValue ruleObj;

    if(ctx->argumentCount() > 0)
        {
        ruleObj = ctx->argument(0);
        }
    else
        {
        return eng->undefinedValue();
        }

    /**
      * First, process the events that are to be hooked on to
      */
    if(ruleObj.property("on").isValid() && !ruleObj.property("on").isUndefined())
        {
        QStringList eventStrings;
        QScriptValueList connectors;
        QScriptValueIterator it(ruleObj.property("on"));

        while (it.hasNext())
            {
            it.next();
            QScriptValue connector = (eng->globalObject()).property("workspace").property(it.value().toString());

            if(connector.isValid() && !connector.isUndefined())
                {
                eventStrings<<it.value().toString();
                connectors<<connector;
                }
            }

        proxy->setConnectors(connectors);
        proxy->setEventStrings(eventStrings);
        }

    /**
      * Then we have our filters.. the most daring part of KWin Scripting yet :)
      * Actually this is pretty simple here, but just wait to see
      * Chelate LazyLogic (tm)
      */
    if(ruleObj.property("filter").isValid() && !ruleObj.property("filter").isUndefined())
        {
        QScriptValue filter = ruleObj.property("filter");

        if(filter.isFunction())
            {
            proxy->setFilter(filter);
            }
        }

    if(ruleObj.property("action").isValid() && !ruleObj.property("action").isUndefined())
        {
        QScriptValue action = ruleObj.property("action");

        if(action.isFunction())
            {
            proxy->setAction(action);
            }
        }

    proxy->setEngine(eng);
    proxy->setId(KWin::Chelate::ChelateProxy::chMap.size());
    KWin::Chelate::ChelateProxy::chMap.append(proxy);
    proxy->processAll();
    return chRule;
    }

QScriptValue KWin::Chelate::publishChelate(QScriptEngine* eng)
    {
    QScriptValue temp = eng->newObject();
    temp.setProperty("rule", eng->newFunction(KWin::Chelate::rule, 1), QScriptValue::Undeletable);

    temp.setProperty("and", KWin::MetaScripting::getLazyLogicFunction(eng, "ll_and"), QScriptValue::Undeletable);
    temp.setProperty("or", KWin::MetaScripting::getLazyLogicFunction(eng, "ll_or"), QScriptValue::Undeletable);
    temp.setProperty("not", KWin::MetaScripting::getLazyLogicFunction(eng, "ll_not"), QScriptValue::Undeletable);

    temp.setProperty("equiv", eng->newFunction(KWin::Chelate::chelationEquivGen, 0), QScriptValue::Undeletable);
    temp.setProperty("regex", eng->newFunction(KWin::Chelate::chelationRegexGen, 0), QScriptValue::Undeletable);
    return temp;
    }

/**
  * Chelation functions. These functions are used to filter
  * the incoming clients.
  */

QScriptValue KWin::Chelate::chelationRegex(QScriptContext* ctx, QScriptEngine* eng)
    {
    QScriptValue key = ((ctx->callee()).data()).property("key");
    QScriptValue regex = ((ctx->callee()).data()).property("regex");
    QScriptValue post = ((ctx->callee()).data()).property("post");
    QRegExp re(regex.toString());

    if(!key.isUndefined() && !regex.isUndefined())
        {
        QScriptValue scCentral = ctx->argument(0);
        QScriptValue callBase = scCentral.property(key.toString());
        QString final = (callBase.isFunction())?((callBase.call(scCentral)).toString()):(callBase.toString());
        int pos = re.indexIn(final);

        if(post.isUndefined())
            {
            if(pos > -1)
                {
                return eng->toScriptValue<bool>(1);
                }
            else
                {
                return eng->toScriptValue<bool>(0);
                }
            }
        else
            {
            QScriptValueList args;
            QStringList list = re.capturedTexts();
            args<<eng->toScriptValue<QStringList>(re.capturedTexts());
            return eng->toScriptValue<bool>((post.call(QScriptValue(), args)).toBool());
            }
        }
    else
        {
        return eng->toScriptValue<bool>(0);
        }
    }

QScriptValue KWin::Chelate::chelationRegexGen(QScriptContext* ctx, QScriptEngine* eng)
    {
    QScriptValue func = eng->newFunction(chelationRegex, 0);
    QScriptValue data = eng->newObject();
    data.setProperty("key", ctx->argument(0));
    data.setProperty("regex", ctx->argument(1));
    data.setProperty("post", ctx->argument(2));
    func.setData(data);
    return func;
    }

QScriptValue KWin::Chelate::chelationEquiv(QScriptContext* ctx, QScriptEngine* eng)
    {
    QScriptValue key = ((ctx->callee()).data()).property("key");
    QScriptValue value = ((ctx->callee()).data()).property("value");

    if(!key.isUndefined() && !value.isUndefined())
        {
        QScriptValue scCentral = ctx->argument(0);
        QScriptValue callBase = scCentral.property(key.toString());
        QString final = (callBase.isFunction())?((callBase.call(scCentral)).toString()):(callBase.toString());

        return eng->toScriptValue<bool>(final.compare(value.toString(), Qt::CaseInsensitive) == 0);
        }
    else
        {
        return eng->toScriptValue<bool>(0);
        }
    }

QScriptValue KWin::Chelate::chelationEquivGen(QScriptContext* ctx, QScriptEngine* eng)
    {
    QScriptValue func = eng->newFunction(chelationEquiv, 0);
    QScriptValue data = eng->newObject();
    data.setProperty("key", ctx->argument(0));
    data.setProperty("value", ctx->argument(1));
    func.setData(data);
    return func;
    }
    
QScriptValue KWin::Chelate::chelationCheck(QScriptContext* ctx, QScriptEngine* eng)
   {
   Q_UNUSED(eng)
   QScriptValue key = ((ctx->callee()).data()).property("key");
   
   if(!key.isUndefined())
       {
       QScriptValue scCentral = ctx->argument(0);
       QScriptValue callBase = scCentral.property(key.toString());
       return (callBase.isFunction())?((callBase.call(scCentral)).toBool()):(callBase.toBool());
       }
       
   return eng->toScriptValue<bool>(0);
   }
    
QScriptValue KWin::Chelate::chelationCheckGen(QScriptContext* ctx, QScriptEngine* eng)
    {
    QScriptValue func = eng->newFunction(chelationCheck, 0);
    QScriptValue data = eng->newObject();
    data.setProperty("key", ctx->argument(0));
    func.setData(data);
    return func;
    }

/**                */
/** LazyLogic (tm) */
/**                */

QScriptValue KWin::Chelate::lazyLogicGenerate(QScriptContext* ctx, QScriptEngine* eng)
    {
    // Assume everything as necessary. If enough or wrong parameters
    // are provided, undefined values are used and then the function call
    // fails silently (it is non-fatal)
    QScriptValue type = (((ctx->callee()).data()).property("lazylogic_type"));
    QString typeString = type.toString();

    if(!((typeString == "ll_and") || (typeString == "ll_or") || (typeString == "ll_not") || (typeString == "ll_xor")))
        {
        return eng->undefinedValue();
        }

    QScriptValue func = eng->newFunction(KWin::Chelate::lazyLogic, 0);
    QScriptValue data = eng->newObject();
    data.setProperty("lazylogic_type", type);
    data.setProperty("lazylogic_operands", ctx->argumentsObject());
    func.setData(data);
    return func;
    }

QScriptValue KWin::Chelate::lazyLogic(QScriptContext* ctx, QScriptEngine* eng)
    {
    bool init;
    QString type = (((ctx->callee()).data()).property("lazylogic_type")).toString();
    QScriptValue operands = ((ctx->callee()).data()).property("lazylogic_operands");

    if(type == "ll_and")
        {
        init = 1;
        }
    else if(type == "ll_or")
        {
        init = 0;
        }
    else if(type == "ll_not")
        {
        QScriptValue current = operands.property("0");

        return !(current.isFunction())?((current.call(QScriptValue(), ctx->argumentsObject())).toBool()):(current.toBool());
        }
    else if(type == "ll_xor")
        {
        QScriptValue _current = operands.property("0");
        QScriptValue  current = operands.property("1");

        return (( current.isFunction())?(( current.call(QScriptValue(), ctx->argumentsObject())).toBool()): current.toBool() ^
                (_current.isFunction())?((_current.call(QScriptValue(), ctx->argumentsObject())).toBool()):_current.toBool()
               );
        }
    else
        {
        return eng->toScriptValue<bool>(0);
        }

    for(int i=0; i<operands.property("length").toNumber(); i++)
        {
        QScriptValue current = operands.property(QString::number(i));
        if(current.isFunction())
            {
            if(type == "ll_and")
                {
                init = init && (current.isFunction())?((current.call(QScriptValue(), ctx->argumentsObject())).toBool()):(current.toBool());
                }
            else if(type == "ll_or")
                {
                init = init || (current.isFunction())?((current.call(QScriptValue(), ctx->argumentsObject())).toBool()):(current.toBool());
                }
            }
        }

    return eng->toScriptValue<bool>(init);
    }

/** */
// END of KWin::Chelate | functions
/** */
