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


KWin::Script::Script(QString scriptName, QDir dir, QObject *parent)
    : QObject(parent)
    , m_engine(new QScriptEngine(this))
    , m_scriptDir(dir)
    , m_configFile(QFileInfo(m_scriptFile).completeBaseName() + QString(".kwscfg"))
    , m_workspace(new SWrapper::Workspace(m_engine))
{
    m_scriptFile.setFileName(dir.filePath(scriptName));
}

KWin::Script::~Script()
{
}

void KWin::Script::run()
{
    if (m_scriptFile.open(QIODevice::ReadOnly)) {
        m_workspace->attach(m_engine);
        m_engine->globalObject().setProperty("QTimer", constructTimerClass(m_engine));
        m_engine->globalObject().setProperty("ClientGroup", SWrapper::ClientGroup::publishClientGroupClass(m_engine));
        m_engine->globalObject().setProperty("chelate", KWin::Chelate::publishChelate(m_engine));
        m_engine->globalObject().setProperty("ch", KWin::Chelate::publishChelate(m_engine));
        QObject::connect(m_engine, SIGNAL(signalHandlerException(QScriptValue)), this, SLOT(sigException(QScriptValue)));
        KWin::MetaScripting::registration(m_engine);

        if (m_scriptDir.exists(m_configFile)) {
            QSettings scriptSettings(m_scriptDir.filePath(m_configFile), QSettings::IniFormat);
            QHash<QString, QVariant> scriptConfig;
            QStringList keys = scriptSettings.allKeys();

            for (int i = 0; i < keys.size(); i++) {
                scriptConfig.insert(keys.at(i), scriptSettings.value(keys.at(i)));
            }

            KWin::MetaScripting::supplyConfig(m_engine, QVariant(scriptConfig));
        } else {
            KWin::MetaScripting::supplyConfig(m_engine);
        }

        QScriptValue ret = m_engine->evaluate(m_scriptFile.readAll());

        if (ret.isError()) {
            sigException(ret);
        }
    }
}

void KWin::Script::sigException(const QScriptValue& exception)
{
    QScriptValue ret = exception;
    if (ret.isError()) {
        kDebug(1212) << "defaultscript encountered an error at [Line " << m_engine->uncaughtExceptionLineNumber() << "]";
        kDebug(1212) << "Message: " << ret.toString();
        kDebug(1212) << "-----------------";

        QScriptValueIterator iter(ret);
        while (iter.hasNext()) {
            iter.next();
            qDebug() << " " << iter.name() << ": " << iter.value().toString();
        }
    }
}


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
        scripts.append(new KWin::Script(scriptsDir.filePath(scriptList.at(i)), scriptsDir, this));
    }

    // Initialize singletons. Currently, only KWin::Workspace.
    SWrapper::Workspace::initialize(KWin::Workspace::self());

    runScripts();
}

void KWin::Scripting::runScripts()
{
    for (int i = 0; i < scripts.size(); i++) {
        scripts.at(i)->run();
    }
}

KWin::Scripting::~Scripting()
{
}
