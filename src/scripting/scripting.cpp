/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Rohan Prabhu <rohan@rohanprabhu.com>
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scripting.h"
// own
#include "dbuscall.h"
#include "desktopbackgrounditem.h"
#include "effect/quickeffect.h"
#include "gesturehandler.h"
#include "screenedgehandler.h"
#include "scriptedquicksceneeffect.h"
#include "scripting_logging.h"
#include "scriptingutils.h"
#include "shortcuthandler.h"
#include "virtualdesktopmodel.h"
#include "windowmodel.h"
#include "windowthumbnailitem.h"
#include "workspace_wrapper.h"

#include "core/output.h"
#include "input.h"
#include "options.h"
#include "screenedge.h"
#include "tiles/tilemanager.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"
// KDE
#include <KConfigGroup>
#include <KConfigPropertyMap>
#include <KGlobalAccel>
#include <KLocalizedContext>
#include <KLocalizedQmlContext>
#include <KPackage/PackageLoader>
// Qt
#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDebug>
#include <QDomDocument>
#include <QFutureWatcher>
#include <QMenu>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QtConcurrentRun>
// QCoro
#include <QCoroCore>
#include <QCoroDBus>

#include "scriptadaptor.h"

static QRect scriptValueToRect(const QJSValue &value)
{
    return QRect(value.property(QStringLiteral("x")).toInt(),
                 value.property(QStringLiteral("y")).toInt(),
                 value.property(QStringLiteral("width")).toInt(),
                 value.property(QStringLiteral("height")).toInt());
}

static QRectF scriptValueToRectF(const QJSValue &value)
{
    return QRectF(value.property(QStringLiteral("x")).toNumber(),
                  value.property(QStringLiteral("y")).toNumber(),
                  value.property(QStringLiteral("width")).toNumber(),
                  value.property(QStringLiteral("height")).toNumber());
}

static QPoint scriptValueToPoint(const QJSValue &value)
{
    return QPoint(value.property(QStringLiteral("x")).toInt(),
                  value.property(QStringLiteral("y")).toInt());
}

static QPointF scriptValueToPointF(const QJSValue &value)
{
    return QPointF(value.property(QStringLiteral("x")).toNumber(),
                   value.property(QStringLiteral("y")).toNumber());
}

static QSize scriptValueToSize(const QJSValue &value)
{
    return QSize(value.property(QStringLiteral("width")).toInt(),
                 value.property(QStringLiteral("height")).toInt());
}

static QSizeF scriptValueToSizeF(const QJSValue &value)
{
    return QSizeF(value.property(QStringLiteral("width")).toNumber(),
                  value.property(QStringLiteral("height")).toNumber());
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

    new ScriptAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Scripting/Script") + QString::number(scriptId()), this, QDBusConnection::ExportAdaptors);
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

KWin::ScriptTimer::ScriptTimer(QObject *parent)
    : QTimer(parent)
{
}

KWin::Script::Script(int id, QString scriptName, QString pluginName, QObject *parent)
    : AbstractScript(id, scriptName, pluginName, parent)
    , m_engine(new QJSEngine(this))
    , m_starting(false)
{
    // TODO: Remove in kwin 6. We have these converters only for compatibility reasons.
    if (!QMetaType::hasRegisteredConverterFunction<QJSValue, QRect>()) {
        QMetaType::registerConverter<QJSValue, QRect>(scriptValueToRect);
    }
    if (!QMetaType::hasRegisteredConverterFunction<QJSValue, QRectF>()) {
        QMetaType::registerConverter<QJSValue, QRectF>(scriptValueToRectF);
    }

    if (!QMetaType::hasRegisteredConverterFunction<QJSValue, QPoint>()) {
        QMetaType::registerConverter<QJSValue, QPoint>(scriptValueToPoint);
    }
    if (!QMetaType::hasRegisteredConverterFunction<QJSValue, QPointF>()) {
        QMetaType::registerConverter<QJSValue, QPointF>(scriptValueToPointF);
    }

    if (!QMetaType::hasRegisteredConverterFunction<QJSValue, QSize>()) {
        QMetaType::registerConverter<QJSValue, QSize>(scriptValueToSize);
    }
    if (!QMetaType::hasRegisteredConverterFunction<QJSValue, QSizeF>()) {
        QMetaType::registerConverter<QJSValue, QSizeF>(scriptValueToSizeF);
    }
}

KWin::Script::~Script()
{
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
    connect(watcher, &QFutureWatcherBase::finished, this, &Script::slotScriptLoadedFromFile);
    watcher->setFuture(QtConcurrent::run(&KWin::Script::loadScriptFromFile, this, fileName()));
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
    QFutureWatcher<QByteArray> *watcher = dynamic_cast<QFutureWatcher<QByteArray> *>(sender());
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

    // Install console functions (e.g. console.assert(), console.log(), etc).
    m_engine->installExtensions(QJSEngine::ConsoleExtension);

    // Make the timer visible to QJSEngine.
    QJSValue timerMetaObject = m_engine->newQMetaObject(&ScriptTimer::staticMetaObject);
    m_engine->globalObject().setProperty("QTimer", timerMetaObject);

    // Expose enums.
    m_engine->globalObject().setProperty(QStringLiteral("KWin"), m_engine->newQMetaObject(&QtScriptWorkspaceWrapper::staticMetaObject));

    // Make the options object visible to QJSEngine.
    QJSValue optionsObject = m_engine->newQObject(options);
    QJSEngine::setObjectOwnership(options, QJSEngine::CppOwnership);
    m_engine->globalObject().setProperty(QStringLiteral("options"), optionsObject);

    // Make the workspace visible to QJSEngine.
    QJSValue workspaceObject = m_engine->newQObject(Scripting::self()->workspaceWrapper());
    QJSEngine::setObjectOwnership(Scripting::self()->workspaceWrapper(), QJSEngine::CppOwnership);
    m_engine->globalObject().setProperty(QStringLiteral("workspace"), workspaceObject);

    QJSValue self = m_engine->newQObject(this);
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);

    static const QStringList globalProperties{
        QStringLiteral("readConfig"),
        QStringLiteral("callDBus"),

        QStringLiteral("registerShortcut"),
        QStringLiteral("registerScreenEdge"),
        QStringLiteral("unregisterScreenEdge"),
        QStringLiteral("registerTouchScreenEdge"),
        QStringLiteral("unregisterTouchScreenEdge"),
        QStringLiteral("registerUserActionsMenu"),
    };

    for (const QString &propertyName : globalProperties) {
        m_engine->globalObject().setProperty(propertyName, self.property(propertyName));
    }

    // Inject assertion functions. It would be better to create a module with all
    // this assert functions or just deprecate them in favor of console.assert().
    QJSValue result = m_engine->evaluate(QStringLiteral(R"(
        function assert(condition, message) {
            console.assert(condition, message || 'Assertion failed');
        }
        function assertTrue(condition, message) {
            console.assert(condition, message || 'Assertion failed');
        }
        function assertFalse(condition, message) {
            console.assert(!condition, message || 'Assertion failed');
        }
        function assertNull(value, message) {
            console.assert(value === null, message || 'Assertion failed');
        }
        function assertNotNull(value, message) {
            console.assert(value !== null, message || 'Assertion failed');
        }
        function assertEquals(expected, actual, message) {
            console.assert(expected === actual, message || 'Assertion failed');
        }
    )"));
    Q_ASSERT(!result.isError());

    result = m_engine->evaluate(QString::fromUtf8(watcher->result()), fileName());
    if (result.isError()) {
        qCWarning(KWIN_SCRIPTING, "%s:%d: error: %s", qPrintable(fileName()),
                  result.property(QStringLiteral("lineNumber")).toInt(),
                  qPrintable(result.property(QStringLiteral("message")).toString()));
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

QVariant KWin::Script::readConfig(const QString &key, const QVariant &defaultValue)
{
    return config().readEntry(key, defaultValue);
}

/**
 * Finds the type signature for a D-Bus method.
 *
 * Calls Intrsospect on the service to get the type signature for the method's
 * arguments.
 *
 * Example:
 *
 * findDBusMethodSignature("org.freedesktop.Notifications",
 * "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Inhibit");
 *
 * Returns: QStringList("s", "s", "a{sv}"), where "s" is a string, and "a{sv}"
 * is a map of string to variant.
 *
 * @param service The D-Bus service name.
 * @param path The D-Bus object path.
 * @param interface The D-Bus interface name.
 * @param method The D-Bus method name.
 * @return A list of type signatures for the method's arguments.
 */
static QCoro::Task<QStringList> findDBusMethodSignature(
    const QString &service,
    const QString &path,
    const QString &interface,
    const QString &method)
{
    QDBusMessage message = QDBusMessage::createMethodCall(service, path, "org.freedesktop.DBus.Introspectable", "Introspect");
    const QDBusReply<QString> reply = co_await QDBusConnection::sessionBus().asyncCall(message);

    if (!reply.isValid()) {
        qWarning() << "Failed to introspect service" << service << "on path" << path << ":" << reply.error().message();
        co_return QStringList();
    }

    QString xml = reply.value();
    QXmlStreamReader reader(xml);
    QStringList argTypes;
    bool inMethod = false;

    while (!reader.atEnd()) {
        reader.readNext();

        if (reader.isStartElement()) {
            if (reader.name().toString() == QLatin1String("interface")) {
                QString interfaceName = reader.attributes().value("name").toString();
                if (interfaceName == interface) {
                    while (!reader.atEnd()) {
                        reader.readNext();

                        if (reader.isStartElement()) {
                            if (reader.name().toString() == QLatin1String("method")) {
                                QString methodName = reader.attributes().value("name").toString();
                                if (methodName == method) {
                                    inMethod = true;
                                }
                            } else if (inMethod && reader.name().toString() == QLatin1String("arg")) {
                                QString direction = reader.attributes().value("direction").toString();
                                if (direction == QLatin1String("in")) {
                                    argTypes.append(reader.attributes().value("type").toString());
                                }
                            }
                        } else if (reader.isEndElement() && reader.name().toString() == QLatin1String("method")) {
                            if (inMethod) {
                                co_return argTypes;
                            }
                        }
                    }
                }
            }
        }
    }

    co_return QStringList();
}

/**
 * Converts a list of JavaScript arguments to a list of D-Bus arguments with the
 * proper types based on the DBus method signature.
 *
 * This is necessary because QJSValue::toVariant() will only convert numbers to
 * either int or double since JavaScript doesn't properly distinguish between
 * different numeric types, but D-Bus methods require specific type signatures.
 *
 * @param jsArguments The JavaScript arguments.
 * @param dbusMethodSignature The D-Bus method signature, e.g. ["s", "i", "a{sv}"].
 * @return The D-Bus arguments with the proper types.
 */
static QVariantList javascriptArgumentsToDBusArguments(
    const QJSValueList jsArguments,
    const QStringList dbusMethodSignature)
{
    QVariantList dbusArguments;
    dbusArguments.reserve(jsArguments.count());

    for (int i = 0; i < jsArguments.count(); ++i) {
        const QJSValue &jsArgument = jsArguments.at(i);
        const QString dbusType = dbusMethodSignature.value(i);

        if (dbusType == QLatin1String("y")) { // BYTE
            dbusArguments << QVariant::fromValue<quint8>(jsArgument.toNumber());
        } else if (dbusType == QLatin1String("n")) { // INT16
            dbusArguments << QVariant::fromValue<qint16>(jsArgument.toNumber());
        } else if (dbusType == QLatin1String("q")) { // UINT16
            dbusArguments << QVariant::fromValue<quint16>(jsArgument.toNumber());
        } else if (dbusType == QLatin1String("i")) { // INT32
            dbusArguments << QVariant::fromValue<qint32>(jsArgument.toNumber());
        } else if (dbusType == QLatin1String("u")) { // UINT32
            dbusArguments << QVariant::fromValue<quint32>(jsArgument.toNumber());
        } else if (dbusType == QLatin1String("x")) { // INT64
            dbusArguments << QVariant::fromValue<qint64>(jsArgument.toNumber());
        } else if (dbusType == QLatin1String("t")) { // UINT64
            dbusArguments << QVariant::fromValue<quint64>(jsArgument.toNumber());
        } else if (dbusType == QLatin1String("d")) { // DOUBLE
            dbusArguments << QVariant::fromValue<double>(jsArgument.toNumber());
        } else {
            // If the D-Bus method signature doesn't match any of the above
            // types, it doesn't need special handling so we can just convert
            // the JavaScript argument to a QVariant.
            dbusArguments << jsArgument.toVariant();
        }
    }

    return dbusArguments;
}

QCoro::Task<void> KWin::Script::handleCallDBus(const QString service, const QString path,
                                               const QString interface, const QString method,
                                               QJSValueList args)
{
    QJSValue callback;

    if (!args.isEmpty() && args.last().isCallable()) {
        callback = args.takeLast();
    }

    QStringList dbusMethodSignature = co_await findDBusMethodSignature(service, path, interface, method);
    QVariantList dbusArguments = javascriptArgumentsToDBusArguments(args, dbusMethodSignature);

    QDBusMessage message = QDBusMessage::createMethodCall(service, path, interface, method);
    message.setArguments(dbusArguments);

    const QDBusPendingReply<> reply = co_await QDBusConnection::sessionBus().asyncCall(message);

    if (reply.isError()) {
        qCWarning(KWIN_SCRIPTING) << "Received D-Bus message is error:" << reply.error().message();
        co_return;
    }

    if (callback.isUndefined()) {
        qCWarning(KWIN_SCRIPTING) << "No callback provided";
        co_return;
    }

    QJSValueList arguments;
    for (const QVariant &variant : reply.reply().arguments()) {
        arguments << m_engine->toScriptValue(dbusToVariant(variant));
    }

    QJSValue(callback).call(arguments);
}

void KWin::Script::callDBus(const QString &service, const QString &path, const QString &interface,
                            const QString &method, const QJSValue &arg1, const QJSValue &arg2,
                            const QJSValue &arg3, const QJSValue &arg4, const QJSValue &arg5,
                            const QJSValue &arg6, const QJSValue &arg7, const QJSValue &arg8,
                            const QJSValue &arg9)
{
    QJSValueList jsArguments;
    jsArguments.reserve(9);

    if (!arg1.isUndefined()) {
        jsArguments << arg1;
    }
    if (!arg2.isUndefined()) {
        jsArguments << arg2;
    }
    if (!arg3.isUndefined()) {
        jsArguments << arg3;
    }
    if (!arg4.isUndefined()) {
        jsArguments << arg4;
    }
    if (!arg5.isUndefined()) {
        jsArguments << arg5;
    }
    if (!arg6.isUndefined()) {
        jsArguments << arg6;
    }
    if (!arg7.isUndefined()) {
        jsArguments << arg7;
    }
    if (!arg8.isUndefined()) {
        jsArguments << arg8;
    }
    if (!arg9.isUndefined()) {
        jsArguments << arg9;
    }

    handleCallDBus(service, path, interface, method, jsArguments);
}

bool KWin::Script::registerShortcut(const QString &objectName, const QString &text, const QString &keySequence, const QJSValue &callback)
{
    if (!callback.isCallable()) {
        m_engine->throwError(QStringLiteral("Shortcut handler must be callable"));
        return false;
    }

    QAction *action = new QAction(this);
    action->setObjectName(objectName);
    action->setText(text);

    const QKeySequence shortcut = keySequence;
    KGlobalAccel::self()->setShortcut(action, {shortcut});

    connect(action, &QAction::triggered, this, [this, action, callback]() {
        QJSValue(callback).call({m_engine->toScriptValue(action)});
    });

    return true;
}

bool KWin::Script::registerScreenEdge(int edge, const QJSValue &callback)
{
    if (!callback.isCallable()) {
        m_engine->throwError(QStringLiteral("Screen edge handler must be callable"));
        return false;
    }

    QJSValueList &callbacks = m_screenEdgeCallbacks[edge];
    if (callbacks.isEmpty()) {
        workspace()->screenEdges()->reserve(static_cast<KWin::ElectricBorder>(edge), this, "slotBorderActivated");
    }

    callbacks << callback;

    return true;
}

bool KWin::Script::unregisterScreenEdge(int edge)
{
    auto it = m_screenEdgeCallbacks.find(edge);
    if (it == m_screenEdgeCallbacks.end()) {
        return false;
    }

    workspace()->screenEdges()->unreserve(static_cast<KWin::ElectricBorder>(edge), this);
    m_screenEdgeCallbacks.erase(it);

    return true;
}

bool KWin::Script::registerTouchScreenEdge(int edge, const QJSValue &callback)
{
    if (!callback.isCallable()) {
        m_engine->throwError(QStringLiteral("Touch screen edge handler must be callable"));
        return false;
    }
    if (m_touchScreenEdgeCallbacks.contains(edge)) {
        return false;
    }

    QAction *action = new QAction(this);
    workspace()->screenEdges()->reserveTouch(KWin::ElectricBorder(edge), action);
    m_touchScreenEdgeCallbacks.insert(edge, action);

    connect(action, &QAction::triggered, this, [callback]() {
        QJSValue(callback).call();
    });

    return true;
}

bool KWin::Script::unregisterTouchScreenEdge(int edge)
{
    auto it = m_touchScreenEdgeCallbacks.find(edge);
    if (it == m_touchScreenEdgeCallbacks.end()) {
        return false;
    }

    delete it.value();
    m_touchScreenEdgeCallbacks.erase(it);

    return true;
}

void KWin::Script::registerUserActionsMenu(const QJSValue &callback)
{
    if (!callback.isCallable()) {
        m_engine->throwError(QStringLiteral("User action handler must be callable"));
        return;
    }
    m_userActionsMenuCallbacks.append(callback);
}

QList<QAction *> KWin::Script::actionsForUserActionMenu(KWin::Window *client, QMenu *parent)
{
    QList<QAction *> actions;
    actions.reserve(m_userActionsMenuCallbacks.count());

    for (QJSValue callback : std::as_const(m_userActionsMenuCallbacks)) {
        const QJSValue result = callback.call({m_engine->toScriptValue(client)});
        if (result.isError()) {
            continue;
        }
        if (!result.isObject()) {
            continue;
        }
        if (QAction *action = scriptValueToAction(result, parent)) {
            actions << action;
        }
    }

    return actions;
}

bool KWin::Script::slotBorderActivated(ElectricBorder border)
{
    const QJSValueList callbacks = m_screenEdgeCallbacks.value(border);
    if (callbacks.isEmpty()) {
        return false;
    }
    std::for_each(callbacks.begin(), callbacks.end(), [](QJSValue callback) {
        callback.call();
    });
    return true;
}

QAction *KWin::Script::scriptValueToAction(const QJSValue &value, QMenu *parent)
{
    const QString title = value.property(QStringLiteral("text")).toString();
    if (title.isEmpty()) {
        return nullptr;
    }

    // Either a menu or a menu item.
    const QJSValue itemsValue = value.property(QStringLiteral("items"));
    if (!itemsValue.isUndefined()) {
        return createMenu(title, itemsValue, parent);
    }

    return createAction(title, value, parent);
}

QAction *KWin::Script::createAction(const QString &title, const QJSValue &item, QMenu *parent)
{
    const QJSValue callback = item.property(QStringLiteral("triggered"));
    if (!callback.isCallable()) {
        return nullptr;
    }

    const bool checkable = item.property(QStringLiteral("checkable")).toBool();
    const bool checked = item.property(QStringLiteral("checked")).toBool();

    QAction *action = new QAction(title, parent);
    action->setCheckable(checkable);
    action->setChecked(checked);

    connect(action, &QAction::triggered, this, [this, action, callback]() {
        QJSValue(callback).call({m_engine->toScriptValue(action)});
    });

    return action;
}

QAction *KWin::Script::createMenu(const QString &title, const QJSValue &items, QMenu *parent)
{
    if (!items.isArray()) {
        return nullptr;
    }

    const int length = items.property(QStringLiteral("length")).toInt();
    if (!length) {
        return nullptr;
    }

    QMenu *menu = new QMenu(title, parent);
    for (int i = 0; i < length; ++i) {
        const QJSValue value = items.property(QString::number(i));
        if (!value.isObject()) {
            continue;
        }
        if (QAction *action = scriptValueToAction(value, menu)) {
            menu->addAction(action);
        }
    }

    return menu->menuAction();
}

KWin::DeclarativeScript::DeclarativeScript(int id, QString scriptName, QString pluginName, QObject *parent)
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
        qCWarning(KWIN_SCRIPTING) << "Component failed to load: " << m_component->errors();
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

KWin::Scripting *KWin::Scripting::s_self = nullptr;

KWin::Scripting *KWin::Scripting::create(QObject *parent)
{
    Q_ASSERT(!s_self);
    s_self = new Scripting(parent);
    return s_self;
}

KWin::Scripting::Scripting(QObject *parent)
    : QObject(parent)
    , m_scriptsLock(new QRecursiveMutex)
    , m_qmlEngine(new QQmlEngine(this))
    , m_declarativeScriptSharedContext(new QQmlContext(m_qmlEngine, this))
    , m_workspaceWrapper(new QtScriptWorkspaceWrapper(this))
{
    m_qmlEngine->setProperty("_kirigamiTheme", QStringLiteral("KirigamiPlasmaStyle"));
    m_qmlEngine->rootContext()->setContextObject(new KLocalizedQmlContext(m_qmlEngine));
    init();
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Scripting"), this, QDBusConnection::ExportScriptableContents | QDBusConnection::ExportScriptableInvokables);
    connect(Workspace::self(), &Workspace::configChanged, this, &Scripting::start);
    connect(Workspace::self(), &Workspace::workspaceInitialized, this, &Scripting::start);
}

void KWin::Scripting::init()
{
    qRegisterMetaType<QList<KWin::Output *>>();
    qRegisterMetaType<QList<KWin::Window *>>();
    qRegisterMetaType<QList<KWin::VirtualDesktop *>>();

    qmlRegisterType<DesktopBackgroundItem>("org.kde.kwin", 3, 0, "DesktopBackground");
    qmlRegisterType<WindowThumbnailItem>("org.kde.kwin", 3, 0, "WindowThumbnail");
    qmlRegisterType<DBusCall>("org.kde.kwin", 3, 0, "DBusCall");
    qmlRegisterType<ScreenEdgeHandler>("org.kde.kwin", 3, 0, "ScreenEdgeHandler");
    qmlRegisterType<ShortcutHandler>("org.kde.kwin", 3, 0, "ShortcutHandler");
    qmlRegisterType<SwipeGestureHandler>("org.kde.kwin", 3, 0, "SwipeGestureHandler");
    qmlRegisterType<PinchGestureHandler>("org.kde.kwin", 3, 0, "PinchGestureHandler");
    qmlRegisterType<WindowModel>("org.kde.kwin", 3, 0, "WindowModel");
    qmlRegisterType<WindowFilterModel>("org.kde.kwin", 3, 0, "WindowFilterModel");
    qmlRegisterType<VirtualDesktopModel>("org.kde.kwin", 3, 0, "VirtualDesktopModel");
    qmlRegisterUncreatableType<KWin::QuickSceneView>("org.kde.kwin", 3, 0, "SceneView", QStringLiteral("Can't instantiate an object of type SceneView"));
    qmlRegisterType<ScriptedQuickSceneEffect>("org.kde.kwin", 3, 0, "SceneEffect");

    qmlRegisterSingletonType<DeclarativeScriptWorkspaceWrapper>("org.kde.kwin", 3, 0, "Workspace", [](QQmlEngine *qmlEngine, QJSEngine *jsEngine) {
        return new DeclarativeScriptWorkspaceWrapper();
    });
    qmlRegisterSingletonInstance("org.kde.kwin", 3, 0, "Options", options);

    qmlRegisterAnonymousType<KConfigPropertyMap>("org.kde.kwin", 3);
    qmlRegisterAnonymousType<KWin::Output>("org.kde.kwin", 3);
    qmlRegisterAnonymousType<KWin::Window>("org.kde.kwin", 3);
    qmlRegisterAnonymousType<KWin::VirtualDesktop>("org.kde.kwin", 3);
    qmlRegisterAnonymousType<QAbstractItemModel>("org.kde.kwin", 3);
    qmlRegisterAnonymousType<KWin::TileManager>("org.kde.kwin", 3);
    // TODO: call the qml types as the C++ types?
    qmlRegisterUncreatableType<KWin::CustomTile>("org.kde.kwin", 3, 0, "CustomTile", QStringLiteral("Cannot create objects of type Tile"));
    qmlRegisterUncreatableType<KWin::Tile>("org.kde.kwin", 3, 0, "Tile", QStringLiteral("Cannot create objects of type AbstractTile"));
}

void KWin::Scripting::start()
{
#if 0
    // TODO make this threaded again once KConfigGroup is sufficiently thread safe, bug #305361 and friends
    // perform querying for the services in a thread
    QFutureWatcher<LoadScriptList> *watcher = new QFutureWatcher<LoadScriptList>(this);
    connect(watcher, &QFutureWatcher<LoadScriptList>::finished, this, &Scripting::slotScriptsQueried);
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
    QMap<QString, QString> pluginStates = KConfigGroup(_config, QStringLiteral("Plugins")).entryMap();

    const QStringList scriptFolders{
        QStringLiteral("kwin-wayland/scripts/"),
        QStringLiteral("kwin/scripts/"),
    };

    LoadScriptList scriptsToLoad;
    for (const QString &scriptFolder : scriptFolders) {
        const auto offers = KPackage::PackageLoader::self()->listPackages(QStringLiteral("KWin/Script"), scriptFolder);
        for (const KPluginMetaData &service : offers) {
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
            // The file we want to load depends on the specified API. We could check if one or the other file exists, but that is more error prone and causes IO overhead
            const QString relScriptPath = scriptFolder + pluginName + QLatin1String("/contents/") + (javaScript ? QLatin1String("code/main.js") : QLatin1String("ui/main.qml"));
            const QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation, relScriptPath);
            if (file.isEmpty()) {
                qCDebug(KWIN_SCRIPTING) << "Could not find script file for " << pluginName;
                continue;
            }
            scriptsToLoad << qMakePair(javaScript, qMakePair(file, pluginName));
        }
    }

    return scriptsToLoad;
}

void KWin::Scripting::slotScriptsQueried()
{
    QFutureWatcher<LoadScriptList> *watcher = dynamic_cast<QFutureWatcher<LoadScriptList> *>(sender());
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
    QMutexLocker locker(m_scriptsLock.get());
    for (AbstractScript *script : std::as_const(scripts)) {
        if (script->pluginName() == pluginName) {
            return script;
        }
    }
    return nullptr;
}

bool KWin::Scripting::unloadScript(const QString &pluginName)
{
    QMutexLocker locker(m_scriptsLock.get());
    for (AbstractScript *script : std::as_const(scripts)) {
        if (script->pluginName() == pluginName) {
            script->deleteLater();
            return true;
        }
    }
    return false;
}

void KWin::Scripting::runScripts()
{
    QMutexLocker locker(m_scriptsLock.get());
    for (int i = 0; i < scripts.size(); i++) {
        scripts.at(i)->run();
    }
}

void KWin::Scripting::scriptDestroyed(QObject *object)
{
    QMutexLocker locker(m_scriptsLock.get());
    scripts.removeAll(static_cast<KWin::Script *>(object));
}

int KWin::Scripting::loadScript(const QString &filePath, const QString &pluginName)
{
    QMutexLocker locker(m_scriptsLock.get());
    if (isScriptLoaded(pluginName)) {
        return -1;
    }
    const int id = scripts.size();
    KWin::Script *script = new KWin::Script(id, filePath, pluginName, this);
    connect(script, &QObject::destroyed, this, &Scripting::scriptDestroyed);
    scripts.append(script);
    return id;
}

int KWin::Scripting::loadDeclarativeScript(const QString &filePath, const QString &pluginName)
{
    QMutexLocker locker(m_scriptsLock.get());
    if (isScriptLoaded(pluginName)) {
        return -1;
    }
    const int id = scripts.size();
    KWin::DeclarativeScript *script = new KWin::DeclarativeScript(id, filePath, pluginName, this);
    connect(script, &QObject::destroyed, this, &Scripting::scriptDestroyed);
    scripts.append(script);
    return id;
}

KWin::Scripting::~Scripting()
{
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/Scripting"));
    s_self = nullptr;
}

QList<QAction *> KWin::Scripting::actionsForUserActionMenu(KWin::Window *c, QMenu *parent)
{
    QList<QAction *> actions;
    for (AbstractScript *s : std::as_const(scripts)) {
        // TODO: Allow declarative scripts to add their own user actions.
        if (Script *script = qobject_cast<Script *>(s)) {
            actions << script->actionsForUserActionMenu(c, parent);
        }
    }
    return actions;
}

#include "moc_scripting.cpp"
