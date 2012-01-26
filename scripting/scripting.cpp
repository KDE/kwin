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

#include "scripting.h"
// own
#include "meta.h"
#include "workspace_wrapper.h"
// KDE
#include <kstandarddirs.h>
#include <KDE/KDebug>
// Qt
#include <QtDBus/QDBusConnection>
#include <QtCore/QSettings>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValue>

QScriptValue kwinScriptPrint(QScriptContext *context, QScriptEngine *engine)
{
    KWin::Script *script = qobject_cast<KWin::Script*>(context->callee().data().toQObject());
    QString result;
    for (int i = 0; i < context->argumentCount(); ++i) {
        if (i > 0) {
            result.append(" ");
        }
        result.append(context->argument(i).toString());
    }
    script->printMessage(result);

    return engine->undefinedValue();
}


KWin::Script::Script(int scriptId, QString scriptName, QDir dir, QObject *parent)
    : QObject(parent)
    , m_scriptId(scriptId)
    , m_engine(new QScriptEngine(this))
    , m_scriptDir(dir)
    , m_configFile(QFileInfo(m_scriptFile).completeBaseName() + QString(".kwscfg"))
    , m_workspace(new WorkspaceWrapper(m_engine))
    , m_running(false)
{
    m_scriptFile.setFileName(dir.filePath(scriptName));
    QDBusConnection::sessionBus().registerObject('/' + QString::number(m_scriptId), this, QDBusConnection::ExportScriptableContents | QDBusConnection::ExportScriptableInvokables);
}

KWin::Script::~Script()
{
    QDBusConnection::sessionBus().unregisterObject('/' + QString::number(m_scriptId));
}

void KWin::Script::printMessage(const QString &message)
{
    kDebug(1212) << m_scriptFile.fileName() << ":" << message;
    emit print(message);
}

void KWin::Script::run()
{
    if (m_running) {
        return;
    }
    if (m_scriptFile.open(QIODevice::ReadOnly)) {
        QScriptValue workspace = m_engine->newQObject(m_workspace, QScriptEngine::QtOwnership,
                                QScriptEngine::ExcludeSuperClassContents | QScriptEngine::ExcludeDeleteLater);
        m_engine->globalObject().setProperty("workspace", workspace, QScriptValue::Undeletable);
        m_engine->globalObject().setProperty("QTimer", constructTimerClass(m_engine));
        m_engine->globalObject().setProperty("KWin", m_engine->newQMetaObject(&WorkspaceWrapper::staticMetaObject));
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
        // add our print
        QScriptValue printFunc = m_engine->newFunction(kwinScriptPrint);
        printFunc.setData(m_engine->newQObject(this));
        m_engine->globalObject().setProperty("print", printFunc);

        QScriptValue ret = m_engine->evaluate(m_scriptFile.readAll());

        if (ret.isError()) {
            sigException(ret);
            deleteLater();
        }
    }
    m_running = true;
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
    emit printError(exception.toString());
}

void KWin::Script::stop()
{
    deleteLater();
}


KWin::Scripting::Scripting(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::sessionBus().registerObject("/Scripting", this, QDBusConnection::ExportScriptableContents | QDBusConnection::ExportScriptableInvokables);
    QDBusConnection::sessionBus().registerService("org.kde.kwin.Scripting");
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
        loadScript(scriptsDir.filePath(scriptList.at(i)));
    }

    runScripts();
}

void KWin::Scripting::runScripts()
{
    for (int i = 0; i < scripts.size(); i++) {
        scripts.at(i)->run();
    }
}

void KWin::Scripting::scriptDestroyed(QObject *object)
{
    scripts.removeAll(static_cast<KWin::Script*>(object));
}

int KWin::Scripting::loadScript(const QString &filePath)
{
    const int id = scripts.size();
    KWin::Script *script = new KWin::Script(id, filePath, scriptsDir, this);
    connect(script, SIGNAL(destroyed(QObject*)), SLOT(scriptDestroyed(QObject*)));
    scripts.append(script);
    return id;
}

KWin::Scripting::~Scripting()
{
    QDBusConnection::sessionBus().unregisterObject("/Scripting");
    QDBusConnection::sessionBus().unregisterService("org.kde.kwin.Scripting");
}

#include "scripting.moc"
