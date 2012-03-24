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
#include "../client.h"
#include "../thumbnailitem.h"
#include "../options.h"
#include "../workspace.h"
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
    KWin::AbstractScript *script = qobject_cast<KWin::Script*>(context->callee().data().toQObject());
    if (!script) {
        return engine->undefinedValue();
    }
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

QScriptValue kwinScriptReadConfig(QScriptContext *context, QScriptEngine *engine)
{
    KWin::AbstractScript *script = qobject_cast<KWin::AbstractScript*>(context->callee().data().toQObject());
    if (!script) {
        return engine->undefinedValue();
    }
    if (context->argumentCount() < 1 || context->argumentCount() > 2) {
        kDebug(1212) << "Incorrect number of arguments";
        return engine->undefinedValue();
    }
    const QString key = context->argument(0).toString();
    QVariant defaultValue;
    if (context->argumentCount() == 2) {
        defaultValue = context->argument(1).toVariant();
    }
    return engine->newVariant(script->config().readEntry(key, defaultValue));
}

KWin::AbstractScript::AbstractScript(int id, QString scriptName, QString pluginName, QObject *parent)
    : QObject(parent)
    , m_scriptId(id)
    , m_pluginName(pluginName)
    , m_running(false)
    , m_workspace(new WorkspaceWrapper(this))
{
    m_scriptFile.setFileName(scriptName);
    if (m_pluginName.isNull()) {
        m_pluginName = scriptName;
    }
}

KWin::AbstractScript::~AbstractScript()
{
}

KConfigGroup KWin::AbstractScript::config() const
{
    return KGlobal::config()->group("Script-" + m_pluginName);
}

void KWin::AbstractScript::stop()
{
    deleteLater();
}

void KWin::AbstractScript::printMessage(const QString &message)
{
    kDebug(1212) << scriptFile().fileName() << ":" << message;
    emit print(message);
}

void KWin::AbstractScript::installScriptFunctions(QScriptEngine* engine)
{
    // add our print
    QScriptValue printFunc = engine->newFunction(kwinScriptPrint);
    printFunc.setData(engine->newQObject(this));
    engine->globalObject().setProperty("print", printFunc);
    // add read config
    QScriptValue configFunc = engine->newFunction(kwinScriptReadConfig);
    configFunc.setData(engine->newQObject(this));
    engine->globalObject().setProperty("readConfig", configFunc);
}

KWin::Script::Script(int id, QString scriptName, QString pluginName, QObject* parent)
    : AbstractScript(id, scriptName, pluginName, parent)
    , m_engine(new QScriptEngine(this))
{
    QDBusConnection::sessionBus().registerObject('/' + QString::number(scriptId()), this, QDBusConnection::ExportScriptableContents | QDBusConnection::ExportScriptableInvokables);
}

KWin::Script::~Script()
{
    QDBusConnection::sessionBus().unregisterObject('/' + QString::number(scriptId()));
}

void KWin::Script::run()
{
    if (running()) {
        return;
    }
    if (scriptFile().open(QIODevice::ReadOnly)) {
        QScriptValue workspace = m_engine->newQObject(AbstractScript::workspace(), QScriptEngine::QtOwnership,
                                QScriptEngine::ExcludeSuperClassContents | QScriptEngine::ExcludeDeleteLater);
        QScriptValue optionsValue = m_engine->newQObject(options, QScriptEngine::QtOwnership,
                                QScriptEngine::ExcludeSuperClassContents | QScriptEngine::ExcludeDeleteLater);
        m_engine->globalObject().setProperty("workspace", workspace, QScriptValue::Undeletable);
        m_engine->globalObject().setProperty("options", optionsValue, QScriptValue::Undeletable);
        m_engine->globalObject().setProperty("QTimer", constructTimerClass(m_engine));
        m_engine->globalObject().setProperty("KWin", m_engine->newQMetaObject(&WorkspaceWrapper::staticMetaObject));
        QObject::connect(m_engine, SIGNAL(signalHandlerException(QScriptValue)), this, SLOT(sigException(QScriptValue)));
        KWin::MetaScripting::registration(m_engine);
        KWin::MetaScripting::supplyConfig(m_engine);
        installScriptFunctions(m_engine);

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

KWin::DeclarativeScript::DeclarativeScript(int id, QString scriptName, QString pluginName, QObject* parent)
    : AbstractScript(id, scriptName, pluginName, parent)
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

    // add read config
    KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(m_view->engine());
    kdeclarative.initialize();
    kdeclarative.setupBindings();
    installScriptFunctions(kdeclarative.scriptEngine());
    qmlRegisterType<ThumbnailItem>("org.kde.kwin", 0, 1, "ThumbnailItem");
    qmlRegisterType<WorkspaceWrapper>("org.kde.kwin", 0, 1, "KWin");
    qmlRegisterType<KWin::Client>();

    m_view->rootContext()->setContextProperty("workspace", workspace());
    m_view->rootContext()->setContextProperty("options", options);

    m_view->setSource(QUrl::fromLocalFile(scriptFile().fileName()));
    setRunning(true);
}

KWin::Scripting::Scripting(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::sessionBus().registerObject("/Scripting", this, QDBusConnection::ExportScriptableContents | QDBusConnection::ExportScriptableInvokables);
    QDBusConnection::sessionBus().registerService("org.kde.kwin.Scripting");
    connect(Workspace::self(), SIGNAL(configChanged()), SLOT(start()));
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
            if (isScriptLoaded(plugininfo.pluginName())) {
                // unload the script
                unloadScript(plugininfo.pluginName());
            }
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
            loadScript(file, pluginName);
        } else if (declarativeScript) {
            loadDeclarativeScript(file, pluginName);
        }
    }

    runScripts();
}

bool KWin::Scripting::isScriptLoaded(const QString &pluginName) const
{
    foreach (AbstractScript *script, scripts) {
        if (script->pluginName() == pluginName) {
            return true;
        }
    }
    return false;
}

bool KWin::Scripting::unloadScript(const QString &pluginName)
{
    foreach (AbstractScript *script, scripts) {
        if (script->pluginName() == pluginName) {
            script->deleteLater();
            return true;
        }
    }
    return false;
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

int KWin::Scripting::loadScript(const QString &filePath, const QString& pluginName)
{
    if (isScriptLoaded(pluginName)) {
        return -1;
    }
    const int id = scripts.size();
    KWin::Script *script = new KWin::Script(id, filePath, pluginName, this);
    connect(script, SIGNAL(destroyed(QObject*)), SLOT(scriptDestroyed(QObject*)));
    scripts.append(script);
    return id;
}

int KWin::Scripting::loadDeclarativeScript(const QString& filePath, const QString& pluginName)
{
    if (isScriptLoaded(pluginName)) {
        return -1;
    }
    const int id = scripts.size();
    KWin::DeclarativeScript *script = new KWin::DeclarativeScript(id, filePath, pluginName, this);
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
