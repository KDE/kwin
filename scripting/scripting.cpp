/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Rohan Prabhu <rohan@rohanprabhu.com>
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scripting.h"
// own
#include "dbuscall.h"
#include "meta.h"
#include "scriptingutils.h"
#include "workspace_wrapper.h"
#include "screenedgeitem.h"
#include "scripting_model.h"
#include "scripting_logging.h"
#include "../x11client.h"
#include "../thumbnailitem.h"
#include "../options.h"
#include "../workspace.h"
// KDE
#include <KConfigGroup>
#include <KPackage/PackageLoader>
// Qt
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDebug>
#include <QFutureWatcher>
#include <QSettings>
#include <QtConcurrentRun>
#include <QMenu>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValue>
#include <QStandardPaths>
#include <QQuickWindow>

QScriptValue kwinScriptPrint(QScriptContext *context, QScriptEngine *engine)
{
    KWin::AbstractScript *script = qobject_cast<KWin::Script*>(context->callee().data().toQObject());
    if (!script) {
        return engine->undefinedValue();
    }
    QString result;
    QTextStream stream(&result);
    for (int i = 0; i < context->argumentCount(); ++i) {
        if (i > 0) {
            stream << " ";
        }
        QScriptValue argument = context->argument(i);
        if (KWin::AbstractClient *client = qscriptvalue_cast<KWin::AbstractClient *>(argument)) {
            stream << client;
        } else {
            stream << argument.toString();
        }
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
        qCDebug(KWIN_SCRIPTING) << "Incorrect number of arguments";
        return engine->undefinedValue();
    }
    const QString key = context->argument(0).toString();
    QVariant defaultValue;
    if (context->argumentCount() == 2) {
        defaultValue = context->argument(1).toVariant();
    }
    return engine->newVariant(script->config().readEntry(key, defaultValue));
}

QScriptValue kwinScriptGlobalShortcut(QScriptContext *context, QScriptEngine *engine)
{
    return KWin::globalShortcut<KWin::AbstractScript*>(context, engine);
}

QScriptValue kwinAssertTrue(QScriptContext *context, QScriptEngine *engine)
{
    return KWin::scriptingAssert<bool>(context, engine, 1, 2, true);
}

QScriptValue kwinAssertFalse(QScriptContext *context, QScriptEngine *engine)
{
    return KWin::scriptingAssert<bool>(context, engine, 1, 2, false);
}

QScriptValue kwinAssertEquals(QScriptContext *context, QScriptEngine *engine)
{
    return KWin::scriptingAssert<QVariant>(context, engine, 2, 3);
}

QScriptValue kwinAssertNull(QScriptContext *context, QScriptEngine *engine)
{
    if (!KWin::validateParameters(context, 1, 2)) {
        return engine->undefinedValue();
    }
    if (!context->argument(0).isNull()) {
        if (context->argumentCount() == 2) {
            context->throwError(QScriptContext::UnknownError, context->argument(1).toString());
        } else {
            context->throwError(QScriptContext::UnknownError,
                                i18nc("Assertion failed in KWin script with given value",
                                      "Assertion failed: %1 is not null", context->argument(0).toString()));
        }
        return engine->undefinedValue();
    }
    return true;
}

QScriptValue kwinAssertNotNull(QScriptContext *context, QScriptEngine *engine)
{
    if (!KWin::validateParameters(context, 1, 2)) {
        return engine->undefinedValue();
    }
    if (context->argument(0).isNull()) {
        if (context->argumentCount() == 2) {
            context->throwError(QScriptContext::UnknownError, context->argument(1).toString());
        } else {
            context->throwError(QScriptContext::UnknownError,
                                i18nc("Assertion failed in KWin script",
                                      "Assertion failed: argument is null"));
        }
        return engine->undefinedValue();
    }
    return true;
}

QScriptValue kwinRegisterScreenEdge(QScriptContext *context, QScriptEngine *engine)
{
    return KWin::registerScreenEdge<KWin::AbstractScript*>(context, engine);
}

QScriptValue kwinUnregisterScreenEdge(QScriptContext *context, QScriptEngine *engine)
{
    return KWin::unregisterScreenEdge<KWin::AbstractScript*>(context, engine);
}

QScriptValue kwinRegisterTouchScreenEdge(QScriptContext *context, QScriptEngine *engine)
{
    return KWin::registerTouchScreenEdge<KWin::Script*>(context, engine);
}

QScriptValue kwinUnregisterTouchScreenEdge(QScriptContext *context, QScriptEngine *engine)
{
    return KWin::unregisterTouchScreenEdge<KWin::Script*>(context, engine);
}

QScriptValue kwinRegisterUserActionsMenu(QScriptContext *context, QScriptEngine *engine)
{
    return KWin::registerUserActionsMenu<KWin::AbstractScript*>(context, engine);
}

QScriptValue kwinCallDBus(QScriptContext *context, QScriptEngine *engine)
{
    KWin::AbstractScript *script = qobject_cast<KWin::AbstractScript*>(context->callee().data().toQObject());
    if (!script) {
        context->throwError(QScriptContext::UnknownError, QStringLiteral("Internal Error: script not registered"));
        return engine->undefinedValue();
    }
    if (context->argumentCount() < 4) {
        context->throwError(QScriptContext::SyntaxError,
                            i18nc("Error in KWin Script",
                                  "Invalid number of arguments. At least service, path, interface and method need to be provided"));
        return engine->undefinedValue();
    }
    if (!KWin::validateArgumentType<QString, QString, QString, QString>(context)) {
        context->throwError(QScriptContext::SyntaxError,
                            i18nc("Error in KWin Script",
                                  "Invalid type. Service, path, interface and method need to be string values"));
        return engine->undefinedValue();
    }
    const QString service = context->argument(0).toString();
    const QString path = context->argument(1).toString();
    const QString interface = context->argument(2).toString();
    const QString method = context->argument(3).toString();
    int argumentsCount = context->argumentCount();
    if (context->argument(argumentsCount-1).isFunction()) {
        --argumentsCount;
    }
    QDBusMessage msg = QDBusMessage::createMethodCall(service, path, interface, method);
    QVariantList arguments;
    for (int i=4; i<argumentsCount; ++i) {
        if (context->argument(i).isArray()) {
            QStringList stringArray = engine->fromScriptValue<QStringList>(context->argument(i));
            arguments << QVariant::fromValue(stringArray);
        } else {
            arguments << context->argument(i).toVariant();
        }
    }
    if (!arguments.isEmpty()) {
        msg.setArguments(arguments);
    }
    if (argumentsCount == context->argumentCount()) {
        // no callback, just fire and forget
        QDBusConnection::sessionBus().asyncCall(msg);
    } else {
        // with a callback
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), script);
        watcher->setProperty("callback", script->registerCallback(context->argument(context->argumentCount()-1)));
        QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), script, SLOT(slotPendingDBusCall(QDBusPendingCallWatcher*)));
    }
    return engine->undefinedValue();
}

KWin::AbstractScript::AbstractScript(int id, QString scriptName, QString pluginName, QObject *parent)
    : QObject(parent)
    , m_scriptId(id)
    , m_fileName(scriptName)
    , m_pluginName(pluginName)
    , m_running(false)
{
    if (m_pluginName.isNull()) {
        m_pluginName = scriptName;
    }
}

KWin::AbstractScript::~AbstractScript()
{
}

KConfigGroup KWin::AbstractScript::config() const
{
    return kwinApp()->config()->group(QLatin1String("Script-") + m_pluginName);
}

void KWin::AbstractScript::stop()
{
    deleteLater();
}

void KWin::AbstractScript::printMessage(const QString &message)
{
    qCDebug(KWIN_SCRIPTING) << fileName() << ":" << message;
    emit print(message);
}

void KWin::AbstractScript::registerShortcut(QAction *a, QScriptValue callback)
{
    m_shortcutCallbacks.insert(a, callback);
    connect(a, SIGNAL(triggered(bool)), SLOT(globalShortcutTriggered()));
}

void KWin::AbstractScript::globalShortcutTriggered()
{
    callGlobalShortcutCallback<KWin::AbstractScript*>(this, sender());
}

bool KWin::AbstractScript::borderActivated(KWin::ElectricBorder edge)
{
    screenEdgeActivated(this, edge);
    return true;
}

void KWin::Script::installScriptFunctions(QScriptEngine* engine)
{
    // add our print
    QScriptValue printFunc = engine->newFunction(kwinScriptPrint);
    printFunc.setData(engine->newQObject(this));
    engine->globalObject().setProperty(QStringLiteral("print"), printFunc);
    // add read config
    QScriptValue configFunc = engine->newFunction(kwinScriptReadConfig);
    configFunc.setData(engine->newQObject(this));
    engine->globalObject().setProperty(QStringLiteral("readConfig"), configFunc);
    QScriptValue dbusCallFunc = engine->newFunction(kwinCallDBus);
    dbusCallFunc.setData(engine->newQObject(this));
    engine->globalObject().setProperty(QStringLiteral("callDBus"), dbusCallFunc);
    // add global Shortcut
    registerGlobalShortcutFunction(this, engine, kwinScriptGlobalShortcut);
    // add screen edge
    registerScreenEdgeFunction(this, engine, kwinRegisterScreenEdge);
    unregisterScreenEdgeFunction(this, engine, kwinUnregisterScreenEdge);
    registerTouchScreenEdgeFunction(this, engine, kwinRegisterTouchScreenEdge);
    unregisterTouchScreenEdgeFunction(this, engine, kwinUnregisterTouchScreenEdge);

    // add user actions menu register function
    registerUserActionsMenuFunction(this, engine, kwinRegisterUserActionsMenu);
    // add assertions
    QScriptValue assertTrueFunc = engine->newFunction(kwinAssertTrue);
    engine->globalObject().setProperty(QStringLiteral("assertTrue"), assertTrueFunc);
    engine->globalObject().setProperty(QStringLiteral("assert"), assertTrueFunc);
    QScriptValue assertFalseFunc = engine->newFunction(kwinAssertFalse);
    engine->globalObject().setProperty(QStringLiteral("assertFalse"), assertFalseFunc);
    QScriptValue assertEqualsFunc = engine->newFunction(kwinAssertEquals);
    engine->globalObject().setProperty(QStringLiteral("assertEquals"), assertEqualsFunc);
    QScriptValue assertNullFunc = engine->newFunction(kwinAssertNull);
    engine->globalObject().setProperty(QStringLiteral("assertNull"), assertNullFunc);
    engine->globalObject().setProperty(QStringLiteral("assertEquals"), assertEqualsFunc);
    QScriptValue assertNotNullFunc = engine->newFunction(kwinAssertNotNull);
    engine->globalObject().setProperty(QStringLiteral("assertNotNull"), assertNotNullFunc);
    // global properties
    engine->globalObject().setProperty(QStringLiteral("KWin"), engine->newQMetaObject(&QtScriptWorkspaceWrapper::staticMetaObject));
    QScriptValue workspace = engine->newQObject(Scripting::self()->workspaceWrapper(), QScriptEngine::QtOwnership,
                                                QScriptEngine::ExcludeDeleteLater);
    engine->globalObject().setProperty(QStringLiteral("workspace"), workspace, QScriptValue::Undeletable);
    // install meta functions
    KWin::MetaScripting::registration(engine);
}

int KWin::AbstractScript::registerCallback(QScriptValue value)
{
    int id = m_callbacks.size();
    m_callbacks.insert(id, value);
    return id;
}

void KWin::AbstractScript::slotPendingDBusCall(QDBusPendingCallWatcher* watcher)
{
    if (watcher->isError()) {
        qCDebug(KWIN_SCRIPTING) << "Received D-Bus message is error";
        watcher->deleteLater();
        return;
    }
    const int id = watcher->property("callback").toInt();
    QDBusMessage reply = watcher->reply();
    QScriptValue callback (m_callbacks.value(id));
    QScriptValueList arguments;
    foreach (const QVariant &argument, reply.arguments()) {
        arguments << callback.engine()->newVariant(argument);
    }
    callback.call(QScriptValue(), arguments);
    m_callbacks.remove(id);
    watcher->deleteLater();
}

void KWin::AbstractScript::registerUseractionsMenuCallback(QScriptValue callback)
{
    m_userActionsMenuCallbacks.append(callback);
}

QList< QAction * > KWin::AbstractScript::actionsForUserActionMenu(KWin::AbstractClient *c, QMenu *parent)
{
    QList<QAction*> returnActions;
    for (QList<QScriptValue>::const_iterator it = m_userActionsMenuCallbacks.constBegin(); it != m_userActionsMenuCallbacks.constEnd(); ++it) {
        QScriptValue callback(*it);
        QScriptValueList arguments;
        arguments << callback.engine()->newQObject(c);
        QScriptValue actions = callback.call(QScriptValue(), arguments);
        if (!actions.isValid() || actions.isUndefined() || actions.isNull()) {
            // script does not want to handle this Client
            continue;
        }
        if (actions.isObject()) {
            QAction *a = scriptValueToAction(actions, parent);
            if (a) {
                returnActions << a;
            }
        }
    }

    return returnActions;
}

QAction *KWin::AbstractScript::scriptValueToAction(QScriptValue &value, QMenu *parent)
{
    QScriptValue titleValue = value.property(QStringLiteral("text"));
    QScriptValue checkableValue = value.property(QStringLiteral("checkable"));
    QScriptValue checkedValue = value.property(QStringLiteral("checked"));
    QScriptValue itemsValue = value.property(QStringLiteral("items"));
    QScriptValue triggeredValue = value.property(QStringLiteral("triggered"));

    if (!titleValue.isValid()) {
        // title not specified - does not make any sense to include
        return nullptr;
    }
    const QString title = titleValue.toString();
    const bool checkable = checkableValue.isValid() && checkableValue.toBool();
    const bool checked = checkable && checkedValue.isValid() && checkedValue.toBool();
    // either a menu or a menu item
    if (itemsValue.isValid()) {
        if (!itemsValue.isArray()) {
            // not an array, so cannot be a menu
            return nullptr;
        }
        QScriptValue lengthValue = itemsValue.property(QStringLiteral("length"));
        if (!lengthValue.isValid() || !lengthValue.isNumber() || lengthValue.toInteger() == 0) {
            // length property missing
            return nullptr;
        }
        return createMenu(title, itemsValue, parent);
    } else if (triggeredValue.isValid()) {
        // normal item
        return createAction(title, checkable, checked, triggeredValue, parent);
    }
    return nullptr;
}

QAction *KWin::AbstractScript::createAction(const QString &title, bool checkable, bool checked, QScriptValue &callback, QMenu *parent)
{
    QAction *action = new QAction(title, parent);
    action->setCheckable(checkable);
    action->setChecked(checked);
    // TODO: rename m_shortcutCallbacks
    m_shortcutCallbacks.insert(action, callback);
    connect(action, SIGNAL(triggered(bool)), SLOT(globalShortcutTriggered()));
    connect(action, SIGNAL(destroyed(QObject*)), SLOT(actionDestroyed(QObject*)));
    return action;
}

QAction *KWin::AbstractScript::createMenu(const QString &title, QScriptValue &items, QMenu *parent)
{
    QMenu *menu = new QMenu(title, parent);
    const int length = static_cast<int>(items.property(QStringLiteral("length")).toInteger());
    for (int i=0; i<length; ++i) {
        QScriptValue value = items.property(QString::number(i));
        if (!value.isValid()) {
            continue;
        }
        if (value.isObject()) {
            QAction *a = scriptValueToAction(value, menu);
            if (a) {
                menu->addAction(a);
            }
        }
    }
    return menu->menuAction();
}

void KWin::AbstractScript::actionDestroyed(QObject *object)
{
    // TODO: Qt 5 - change to lambda function
    m_shortcutCallbacks.remove(static_cast<QAction*>(object));
}

KWin::Script::Script(int id, QString scriptName, QString pluginName, QObject* parent)
    : AbstractScript(id, scriptName, pluginName, parent)
    , m_engine(new QScriptEngine(this))
    , m_starting(false)
    , m_agent(new ScriptUnloaderAgent(this))
{
    QDBusConnection::sessionBus().registerObject(QLatin1Char('/') + QString::number(scriptId()), this, QDBusConnection::ExportScriptableContents | QDBusConnection::ExportScriptableInvokables);
}

KWin::Script::~Script()
{
    QDBusConnection::sessionBus().unregisterObject(QLatin1Char('/') + QString::number(scriptId()));
}

void KWin::Script::run()
{
    if (running() || m_starting) {
        return;
    }

    if (calledFromDBus()) {
        m_invocationContext = message();
        setDelayedReply(true);
    }

    m_starting = true;
    QFutureWatcher<QByteArray> *watcher = new QFutureWatcher<QByteArray>(this);
    connect(watcher, SIGNAL(finished()), SLOT(slotScriptLoadedFromFile()));
    watcher->setFuture(QtConcurrent::run(this, &KWin::Script::loadScriptFromFile, fileName()));
}

QByteArray KWin::Script::loadScriptFromFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    QByteArray result(file.readAll());
    return result;
}

void KWin::Script::slotScriptLoadedFromFile()
{
    QFutureWatcher<QByteArray> *watcher = dynamic_cast< QFutureWatcher< QByteArray>* >(sender());
    if (!watcher) {
        // not invoked from a QFutureWatcher
        return;
    }
    if (watcher->result().isNull()) {
        // do not load empty script
        deleteLater();
        watcher->deleteLater();

        if (m_invocationContext.type() == QDBusMessage::MethodCallMessage) {
            auto reply = m_invocationContext.createErrorReply("org.kde.kwin.Scripting.FileError", QString("Could not open %1").arg(fileName()));
            QDBusConnection::sessionBus().send(reply);
            m_invocationContext = QDBusMessage();
        }

        return;
    }

    QScriptValue optionsValue = m_engine->newQObject(options, QScriptEngine::QtOwnership,
                            QScriptEngine::ExcludeSuperClassContents | QScriptEngine::ExcludeDeleteLater);
    m_engine->globalObject().setProperty(QStringLiteral("options"), optionsValue, QScriptValue::Undeletable);
    m_engine->globalObject().setProperty(QStringLiteral("QTimer"), constructTimerClass(m_engine));
    QObject::connect(m_engine, SIGNAL(signalHandlerException(QScriptValue)), this, SLOT(sigException(QScriptValue)));
    KWin::MetaScripting::supplyConfig(m_engine);
    installScriptFunctions(m_engine);

    QScriptValue ret = m_engine->evaluate(QString::fromUtf8(watcher->result()));

    if (ret.isError()) {
        sigException(ret);
        deleteLater();
    }

    if (m_invocationContext.type() == QDBusMessage::MethodCallMessage) {
        auto reply = m_invocationContext.createReply();
        QDBusConnection::sessionBus().send(reply);
        m_invocationContext = QDBusMessage();
    }

    watcher->deleteLater();
    setRunning(true);
    m_starting = false;
}

void KWin::Script::sigException(const QScriptValue& exception)
{
    QScriptValue ret = exception;
    if (ret.isError()) {
        qCDebug(KWIN_SCRIPTING) << "defaultscript encountered an error at [Line " << m_engine->uncaughtExceptionLineNumber() << "]";
        qCDebug(KWIN_SCRIPTING) << "Message: " << ret.toString();
        qCDebug(KWIN_SCRIPTING) << "-----------------";

        QScriptValueIterator iter(ret);
        while (iter.hasNext()) {
            iter.next();
            qCDebug(KWIN_SCRIPTING) << " " << iter.name() << ": " << iter.value().toString();
        }
    }
    emit printError(exception.toString());
    stop();
}

bool KWin::Script::registerTouchScreenCallback(int edge, QScriptValue callback)
{
    if (m_touchScreenEdgeCallbacks.constFind(edge) != m_touchScreenEdgeCallbacks.constEnd()) {
        return false;
    }
    QAction *action = new QAction(this);
    connect(action, &QAction::triggered, this,
        [callback] {
            QScriptValue invoke(callback);
            invoke.call();
        }
    );
    ScreenEdges::self()->reserveTouch(KWin::ElectricBorder(edge), action);
    m_touchScreenEdgeCallbacks.insert(edge, action);
    return true;
}

bool KWin::Script::unregisterTouchScreenCallback(int edge)
{
    auto it = m_touchScreenEdgeCallbacks.find(edge);
    if (it == m_touchScreenEdgeCallbacks.end()) {
        return false;
    }
    delete it.value();
    m_touchScreenEdgeCallbacks.erase(it);
    return true;
}

KWin::ScriptUnloaderAgent::ScriptUnloaderAgent(KWin::Script *script)
    : QScriptEngineAgent(script->engine())
    , m_script(script)
{
    script->engine()->setAgent(this);
}

void KWin::ScriptUnloaderAgent::scriptUnload(qint64 id)
{
    Q_UNUSED(id)
    m_script->stop();
}

KWin::DeclarativeScript::DeclarativeScript(int id, QString scriptName, QString pluginName, QObject* parent)
    : AbstractScript(id, scriptName, pluginName, parent)
    , m_context(new QQmlContext(Scripting::self()->declarativeScriptSharedContext(), this))
    , m_component(new QQmlComponent(Scripting::self()->qmlEngine(), this))
{
    m_context->setContextProperty(QStringLiteral("KWin"), new JSEngineGlobalMethodsWrapper(this));
}

KWin::DeclarativeScript::~DeclarativeScript()
{
}

void KWin::DeclarativeScript::run()
{
    if (running()) {
        return;
    }

    m_component->loadUrl(QUrl::fromLocalFile(fileName()));
    if (m_component->isLoading()) {
        connect(m_component, &QQmlComponent::statusChanged, this, &DeclarativeScript::createComponent);
    } else {
        createComponent();
    }
}

void KWin::DeclarativeScript::createComponent()
{
    if (m_component->isError()) {
        qCDebug(KWIN_SCRIPTING) << "Component failed to load: " << m_component->errors();
    } else {
        if (QObject *object = m_component->create(m_context)) {
            object->setParent(this);
        }
    }
    setRunning(true);
}

KWin::JSEngineGlobalMethodsWrapper::JSEngineGlobalMethodsWrapper(KWin::DeclarativeScript *parent)
    : QObject(parent)
    , m_script(parent)
{
}

KWin::JSEngineGlobalMethodsWrapper::~JSEngineGlobalMethodsWrapper()
{
}

QVariant KWin::JSEngineGlobalMethodsWrapper::readConfig(const QString &key, QVariant defaultValue)
{
    return m_script->config().readEntry(key, defaultValue);
}

void KWin::JSEngineGlobalMethodsWrapper::registerWindow(QQuickWindow *window)
{
    connect(window, &QWindow::visibilityChanged, this, [window](QWindow::Visibility visibility) {
        if (visibility == QWindow::Hidden) {
            window->destroy();
        }
    }, Qt::QueuedConnection);
}

bool KWin::JSEngineGlobalMethodsWrapper::registerShortcut(const QString &name, const QString &text, const QKeySequence& keys, QJSValue function)
{
    if (!function.isCallable()) {
        qCDebug(KWIN_SCRIPTING) << "Fourth and final argument must be a javascript function";
        return false;
    }

    QAction *a = new QAction(this);
    a->setObjectName(name);
    a->setText(text);
    const QKeySequence shortcut = QKeySequence(keys);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>{shortcut});
    KWin::input()->registerShortcut(shortcut, a);

    connect(a, &QAction::triggered, this, [=]() mutable {
        QJSValueList arguments;
        arguments << Scripting::self()->qmlEngine()->toScriptValue(a);
        function.call(arguments);
    });
    return true;
}

KWin::Scripting *KWin::Scripting::s_self = nullptr;

KWin::Scripting *KWin::Scripting::create(QObject *parent)
{
    Q_ASSERT(!s_self);
    s_self = new Scripting(parent);
    return s_self;
}

KWin::Scripting::Scripting(QObject *parent)
    : QObject(parent)
    , m_scriptsLock(new QMutex(QMutex::Recursive))
    , m_qmlEngine(new QQmlEngine(this))
    , m_declarativeScriptSharedContext(new QQmlContext(m_qmlEngine, this))
    , m_workspaceWrapper(new QtScriptWorkspaceWrapper(this))
{
    init();
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Scripting"), this, QDBusConnection::ExportScriptableContents | QDBusConnection::ExportScriptableInvokables);
    connect(Workspace::self(), SIGNAL(configChanged()), SLOT(start()));
    connect(Workspace::self(), SIGNAL(workspaceInitialized()), SLOT(start()));
}

void KWin::Scripting::init()
{
    qmlRegisterType<DesktopThumbnailItem>("org.kde.kwin", 2, 0, "DesktopThumbnailItem");
    qmlRegisterType<WindowThumbnailItem>("org.kde.kwin", 2, 0, "ThumbnailItem");
    qmlRegisterType<DBusCall>("org.kde.kwin", 2, 0, "DBusCall");
    qmlRegisterType<ScreenEdgeItem>("org.kde.kwin", 2, 0, "ScreenEdgeItem");
    qmlRegisterType<KWin::ScriptingClientModel::ClientModel>();
    qmlRegisterType<KWin::ScriptingClientModel::SimpleClientModel>("org.kde.kwin", 2, 0, "ClientModel");
    qmlRegisterType<KWin::ScriptingClientModel::ClientModelByScreen>("org.kde.kwin", 2, 0, "ClientModelByScreen");
    qmlRegisterType<KWin::ScriptingClientModel::ClientModelByScreenAndDesktop>("org.kde.kwin", 2, 0, "ClientModelByScreenAndDesktop");
    qmlRegisterType<KWin::ScriptingClientModel::ClientModelByScreenAndActivity>("org.kde.kwin", 2, 1, "ClientModelByScreenAndActivity");
    qmlRegisterType<KWin::ScriptingClientModel::ClientFilterModel>("org.kde.kwin", 2, 0, "ClientFilterModel");
    qmlRegisterType<KWin::AbstractClient>();
    qmlRegisterType<KWin::X11Client>();
    qmlRegisterType<QAbstractItemModel>();

    m_qmlEngine->rootContext()->setContextProperty(QStringLiteral("workspace"), m_workspaceWrapper);
    m_qmlEngine->rootContext()->setContextProperty(QStringLiteral("options"), options);

    m_declarativeScriptSharedContext->setContextProperty(QStringLiteral("workspace"), new DeclarativeScriptWorkspaceWrapper(this));
    // QQmlListProperty interfaces only work via properties, rebind them as functions here
    QQmlExpression expr(m_declarativeScriptSharedContext, nullptr, "workspace.clientList = function() { return workspace.clients }");
    expr.evaluate();
}

void KWin::Scripting::start()
{
#if 0
    // TODO make this threaded again once KConfigGroup is sufficiently thread safe, bug #305361 and friends
    // perform querying for the services in a thread
    QFutureWatcher<LoadScriptList> *watcher = new QFutureWatcher<LoadScriptList>(this);
    connect(watcher, SIGNAL(finished()), this, SLOT(slotScriptsQueried()));
    watcher->setFuture(QtConcurrent::run(this, &KWin::Scripting::queryScriptsToLoad, pluginStates, offers));
#else
    LoadScriptList scriptsToLoad = queryScriptsToLoad();
    for (LoadScriptList::const_iterator it = scriptsToLoad.constBegin();
            it != scriptsToLoad.constEnd();
            ++it) {
        if (it->first) {
            loadScript(it->second.first, it->second.second);
        } else {
            loadDeclarativeScript(it->second.first, it->second.second);
        }
    }

    runScripts();
#endif
}

LoadScriptList KWin::Scripting::queryScriptsToLoad()
{
    KSharedConfig::Ptr _config = kwinApp()->config();
    static bool s_started = false;
    if (s_started) {
        _config->reparseConfiguration();
    } else {
        s_started = true;
    }
    QMap<QString,QString> pluginStates = KConfigGroup(_config, "Plugins").entryMap();
    const QString scriptFolder = QStringLiteral(KWIN_NAME "/scripts/");
    const auto offers = KPackage::PackageLoader::self()->listPackages(QStringLiteral("KWin/Script"), scriptFolder);

    LoadScriptList scriptsToLoad;

    for (const KPluginMetaData &service: offers) {
        const QString value = pluginStates.value(service.pluginId() + QLatin1String("Enabled"), QString());
        const bool enabled = value.isNull() ? service.isEnabledByDefault() : QVariant(value).toBool();
        const bool javaScript = service.value(QStringLiteral("X-Plasma-API")) == QLatin1String("javascript");
        const bool declarativeScript = service.value(QStringLiteral("X-Plasma-API")) == QLatin1String("declarativescript");
        if (!javaScript && !declarativeScript) {
            continue;
        }

        if (!enabled) {
            if (isScriptLoaded(service.pluginId())) {
                // unload the script
                unloadScript(service.pluginId());
            }
            continue;
        }
        const QString pluginName = service.pluginId();
        const QString scriptName = service.value(QStringLiteral("X-Plasma-MainScript"));
        const QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation, scriptFolder + pluginName + QLatin1String("/contents/") + scriptName);
        if (file.isNull()) {
            qCDebug(KWIN_SCRIPTING) << "Could not find script file for " << pluginName;
            continue;
        }
        scriptsToLoad << qMakePair(javaScript, qMakePair(file, pluginName));
    }
    return scriptsToLoad;
}

void KWin::Scripting::slotScriptsQueried()
{
    QFutureWatcher<LoadScriptList> *watcher = dynamic_cast< QFutureWatcher<LoadScriptList>* >(sender());
    if (!watcher) {
        // slot invoked not from a FutureWatcher
        return;
    }

    LoadScriptList scriptsToLoad = watcher->result();
    for (LoadScriptList::const_iterator it = scriptsToLoad.constBegin();
            it != scriptsToLoad.constEnd();
            ++it) {
        if (it->first) {
            loadScript(it->second.first, it->second.second);
        } else {
            loadDeclarativeScript(it->second.first, it->second.second);
        }
    }

    runScripts();
    watcher->deleteLater();
}

bool KWin::Scripting::isScriptLoaded(const QString &pluginName) const
{
    return findScript(pluginName) != nullptr;
}

KWin::AbstractScript *KWin::Scripting::findScript(const QString &pluginName) const
{
    QMutexLocker locker(m_scriptsLock.data());
    foreach (AbstractScript *script, scripts) {
        if (script->pluginName() == pluginName) {
            return script;
        }
    }
    return nullptr;
}

bool KWin::Scripting::unloadScript(const QString &pluginName)
{
    QMutexLocker locker(m_scriptsLock.data());
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
    QMutexLocker locker(m_scriptsLock.data());
    for (int i = 0; i < scripts.size(); i++) {
        scripts.at(i)->run();
    }
}

void KWin::Scripting::scriptDestroyed(QObject *object)
{
    QMutexLocker locker(m_scriptsLock.data());
    scripts.removeAll(static_cast<KWin::Script*>(object));
}

int KWin::Scripting::loadScript(const QString &filePath, const QString& pluginName)
{
    QMutexLocker locker(m_scriptsLock.data());
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
    QMutexLocker locker(m_scriptsLock.data());
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
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/Scripting"));
    s_self = nullptr;
}

QList< QAction * > KWin::Scripting::actionsForUserActionMenu(KWin::AbstractClient *c, QMenu *parent)
{
    QList<QAction*> actions;
    foreach (AbstractScript *script, scripts) {
        actions << script->actionsForUserActionMenu(c, parent);
    }
    return actions;
}

