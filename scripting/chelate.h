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

/********************************************************************
             *                 ***
           **                   ***                   *
           **                    **                  **
           **                    **                  **
           **                    **                ********
   ****    **  ***      ***      **       ****    ********     ***
  * ***  * ** * ***    * ***     **      * ***  *    **       * ***
 *   ****  ***   ***  *   ***    **     *   ****     **      *   ***
**         **     ** **    ***   **    **    **      **     **    ***
**         **     ** ********    **    **    **      **     ********
**         **     ** *******     **    **    **      **     *******
**         **     ** **          **    **    **      **     **
***     *  **     ** ****    *   **    **    **      **     ****    *
 *******   **     **  *******    *** *  ***** **      **     *******
  *****     **    **   *****      ***    ***   **             *****
                  *
                 *     The most exciting thing that ever happened to
                *      KWin::Scripting. With LazyLogic (tm)
               *
*********************************************************************/

#ifndef __KWIN__SCRIPTING_CHELATE_H
#define __KWIN__SCRIPTING_CHELATE_H

#include <QScriptClass>
#include <QList>
#include <QScriptEngine>

#include "workspace.h"
#include "client.h"

/**
  * Chelate forms a very different kind of singleton structure.
  * It's functions are designed to mostly provide ample space
  * and environment for lazy evaluations. Chelate is mostly cute.
  */

namespace KWin
{
namespace Chelate
{
/**
  * A class used to store data about each chelate rule.
  */
class ChelateProxy
    {
    private:
        QStringList on;
        QScriptValueList connectors;
        QScriptValue filter;
        QScriptValue action;

        int id;
        QScriptEngine* engine;

    public:
        void setAction(QScriptValue);
        void setFilter(QScriptValue);
        void setEventStrings(QStringList);
        void setConnectors(QScriptValueList);
        void setEngine(QScriptEngine*);
        void setId(int);

        QScriptValue getAction();
        QScriptValue getFilter();

        static QList<ChelateProxy*> chMap;

        void processAll();
        void process(int);

        ChelateProxy();
    };

QScriptValue scanner(QScriptContext*, QScriptEngine*);
QScriptValue rule(QScriptContext*, QScriptEngine*);
QScriptValue publishChelate(QScriptEngine* engine);

/** Chelation */

QScriptValue chelationEquivGen(QScriptContext*, QScriptEngine*);
QScriptValue chelationEquiv(QScriptContext*, QScriptEngine*);

QScriptValue chelationCheckGen(QScriptContext*, QScriptEngine*);
QScriptValue chelationCheck(QScriptContext*, QScriptEngine*);

QScriptValue chelationRegexGen(QScriptContext*, QScriptEngine*);
QScriptValue chelationRegex(QScriptContext*, QScriptEngine*);

/**                */
/** LazyLogic (tm) */
/**                */

QScriptValue lazyLogicGenerate(QScriptContext*, QScriptEngine*);
QScriptValue lazyLogic(QScriptContext*, QScriptEngine*);

};
};

#endif
