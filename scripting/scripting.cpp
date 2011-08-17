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

#include "scripting.h"
#include <kstandarddirs.h>

KWin::Scripting::Scripting()
{
    // Default constructor no longer used, scripting can
    // be disabled by calling kwin --noscript
}

void KWin::Scripting::start()
{
    QStringList scriptFilters;
    QString sDirectory = KStandardDirs::locateLocal("data", "kwin/scripts/");

    if (sDirectory.isEmpty()) {
        // Abort scripting setup. No location found to locate scripts
        return;
    }

    scriptFilters << "*.kwinscript" << "*.kws" << "*.kwinqs";
    scriptsDir.setPath(sDirectory);
    scriptList = scriptsDir.entryList(scriptFilters, QDir::Files | QDir::Readable | QDir::Executable);

    for (int i = 0; i < scriptList.size(); i++) {
        scripts.append(new KWin::Script(new QScriptEngine(), scriptsDir.filePath(scriptList.at(i)), scriptsDir));
    }

    // Initialize singletons. Currently, only KWin::Workspace.
    SWrapper::Workspace::initialize(KWin::Workspace::self());

    runScripts();
}

void KWin::Scripting::runScript(KWin::Script* script)
{
    if (script->scriptFile.open(QIODevice::ReadOnly)) {
        script->workspace = new SWrapper::Workspace(script->engine);
        (script->workspace)->attach(script->engine);
        ((script->engine)->globalObject()).setProperty("QTimer", constructTimerClass((script->engine)));
        ((script->engine)->globalObject()).setProperty("ClientGroup", SWrapper::ClientGroup::publishClientGroupClass((script->engine)));
        ((script->engine)->globalObject()).setProperty("chelate", KWin::Chelate::publishChelate(script->engine));
        ((script->engine)->globalObject()).setProperty("ch", KWin::Chelate::publishChelate(script->engine));
        QObject::connect((script->engine), SIGNAL(signalHandlerException(QScriptValue)), this, SLOT(sigException(QScriptValue)));
        KWin::MetaScripting::registration(script->engine);

        if (scriptsDir.exists(script->configFile)) {
            QSettings scriptSettings(scriptsDir.filePath(script->configFile), QSettings::IniFormat);
            QHash<QString, QVariant> scriptConfig;
            QStringList keys = scriptSettings.allKeys();

            for (int i = 0; i < keys.size(); i++) {
                scriptConfig.insert(keys.at(i), scriptSettings.value(keys.at(i)));
            }

            KWin::MetaScripting::supplyConfig(script->engine, QVariant(scriptConfig));
        } else {
            KWin::MetaScripting::supplyConfig(script->engine);
        }

        QScriptValue ret = (script->engine)->evaluate(QString((script->scriptFile).readAll()));

        if (ret.isError()) {
            sigException(ret);
        }
    }
}


void KWin::Scripting::runScripts()
{
    for (int i = 0; i < scripts.size(); i++) {
        runScript(scripts.at(i));
    }
}

void KWin::Scripting::sigException(const QScriptValue& exception)
{
    QScriptValue ret = exception;
    if (ret.isError()) {
        kDebug(1212) << "defaultscript encountered an error at [Line " << uncaughtExceptionLineNumber() << "]";
        kDebug(1212) << "Message: " << ret.toString();
        kDebug(1212) << "-----------------";

        QScriptValueIterator iter(ret);
        while (iter.hasNext()) {
            iter.next();
            qDebug() << " " << iter.name() << ": " << iter.value().toString();
        }
    }
}

KWin::Scripting::~Scripting()
{
    for (int i = 0; i < scripts.size(); i++) {
        delete scripts.at(i);
    }
}
