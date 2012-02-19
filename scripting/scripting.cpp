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
#include "../thumbnailitem.h"
// KDE
#include <kstandarddirs.h>
#include <KDE/KConfigGroup>
#include <KDE/KDebug>
#include <KDE/KPluginInfo>
#include <KDE/KServiceTypeTrader>
#include <kdeclarative.h>
// Qt
#include <QtDBus/QDBusConnection>
#include <QtCore/QSettings>
#include <QtDeclarative/QDeclarativeContext>
#include <QtDeclarative/QDeclarativeEngine>
#include <QtDeclarative/QDeclarativeView>
#include <QtDeclarative/qdeclarative.h>
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

KWin::AbstractScript::AbstractScript (int id, QString scriptName, QObject *parent)
    : QObject(parent)
    , m_scriptId(id)
    , m_running(false)
    , m_workspace(new WorkspaceWrapper(this))
{
    m_scriptFile.setFileName(scriptName);
}

KWin::AbstractScript::~AbstractScript()
{
}

void KWin::AbstractScript::stop()
{
    deleteLater();
}

KWin::Script::Script(int id, QString scriptName, QObject *parent)
    : AbstractScript(id, scriptName, parent)
    , m_engine(new QScriptEngine(this))
{
    QDBusConnection::sessionBus().registerObject('/' + QString::number(scriptId()), this, QDBusConnection::ExportScriptableContents | QDBusConnection::ExportScriptableInvokables);
}

KWin::Script::~Script()
{
    QDBusConnection::sessionBus().unregisterObject('/' + QString::number(scriptId()));
}

void KWin::Script::printMessage(const QString &message)
{
    kDebug(1212) << scriptFile().fileName() << ":" << message;
    emit print(message);
}

void KWin::Script::run()
{
    if (running()) {
        return;
    }
    if (scriptFile().open(QIODevice::ReadOnly)) {
        QScriptValue workspace = m_engine->newQObject(AbstractScript::workspace(), QScriptEngine::QtOwnership,
                                QScriptEngine::ExcludeSuperClassContents | QScriptEngine::ExcludeDeleteLater);
        m_engine->globalObject().setProperty("workspace", workspace, QScriptValue::Undeletable);
        m_engine->globalObject().setProperty("QTimer", constructTimerClass(m_engine));
        m_engine->globalObject().setProperty("KWin", m_engine->newQMetaObject(&WorkspaceWrapper::staticMetaObject));
        QObject::connect(m_engine, SIGNAL(signalHandlerException(QScriptValue)), this, SLOT(sigException(QScriptValue)));
        KWin::MetaScripting::registration(m_engine);
        KWin::MetaScripting::supplyConfig(m_engine);
        // add our print
        QScriptValue printFunc = m_engine->newFunction(kwinScriptPrint);
        printFunc.setData(m_engine->newQObject(this));
        m_engine->globalObject().setProperty("print", printFunc);

        QScriptValue ret = m_engine->evaluate(scriptFile().readAll());

        if (ret.isError()) {
            sigException(ret);
            deleteLater();
        }
    }
    setRunning(true);
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

KWin::DeclarativeScript::DeclarativeScript(int id, QString scriptName, QObject *parent)
    : AbstractScript(id, scriptName, parent)
    , m_view(new QDeclarativeView())
{
}

KWin::DeclarativeScript::~DeclarativeScript()
{
}

void KWin::DeclarativeScript::run()
{
    if (running()) {
        return;
    }
    m_view->setAttribute(Qt::WA_TranslucentBackground);
    m_view->setWindowFlags(Qt::X11BypassWindowManagerHint);
    m_view->setResizeMode(QDeclarativeView::SizeViewToRootObject);
    QPalette pal = m_view->palette();
    pal.setColor(m_view->backgroundRole(), Qt::transparent);
    m_view->setPalette(pal);


    foreach (const QString &importPath, KGlobal::dirs()->findDirs("module", "imports")) {
        m_view->engine()->addImportPath(importPath);
    }
    KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(m_view->engine());
    kdeclarative.initialize();
    kdeclarative.setupBindings();
    qmlRegisterType<ThumbnailItem>("org.kde.kwin", 0, 1, "ThumbnailItem");
    qmlRegisterType<WorkspaceWrapper>("org.kde.kwin", 0, 1, "KWin");

    m_view->rootContext()->setContextProperty("workspace", workspace());

    m_view->setSource(QUrl::fromLocalFile(scriptFile().fileName()));
    setRunning(true);
}

KWin::Scripting::Scripting(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::sessionBus().registerObject("/Scripting", this, QDBusConnection::ExportScriptableContents | QDBusConnection::ExportScriptableInvokables);
    QDBusConnection::sessionBus().registerService("org.kde.kwin.Scripting");
}

void KWin::Scripting::start()
{
    KSharedConfig::Ptr _config = KGlobal::config();
    KConfigGroup conf(_config, "Plugins");

    KService::List offers = KServiceTypeTrader::self()->query("KWin/Script");

    foreach (const KService::Ptr & service, offers) {
        KPluginInfo plugininfo(service);
        plugininfo.load(conf);
        const bool javaScript = service->property("X-Plasma-API").toString() == "javascript";
        const bool declarativeScript = service->property("X-Plasma-API").toString() == "declarativescript";
        if (!javaScript && !declarativeScript) {
            continue;
        }

        if (!plugininfo.isPluginEnabled()) {
            continue;
        }
        const QString pluginName = service->property("X-KDE-PluginInfo-Name").toString();
        const QString scriptName = service->property("X-Plasma-MainScript").toString();
        const QString file = KStandardDirs::locate("data", "kwin/scripts/" + pluginName + "/contents/" + scriptName);
        if (file.isNull()) {
            kDebug(1212) << "Could not find script file for " << pluginName;
            continue;
        }
        if (javaScript) {
            loadScript(file);
        } else if (declarativeScript) {
            loadDeclarativeScript(file);
        }
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
    KWin::Script *script = new KWin::Script(id, filePath, this);
    connect(script, SIGNAL(destroyed(QObject*)), SLOT(scriptDestroyed(QObject*)));
    scripts.append(script);
    return id;
}

int KWin::Scripting::loadDeclarativeScript(const QString &filePath)
{
    const int id = scripts.size();
    KWin::DeclarativeScript *script = new KWin::DeclarativeScript(id, filePath, this);
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
