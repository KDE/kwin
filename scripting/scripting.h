/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Rohan Prabhu <rohan@rohanprabhu.com>
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_SCRIPTING_H
#define KWIN_SCRIPTING_H

#include <kwinglobals.h>

#include <QFile>
#include <QHash>
#include <QStringList>
#include <QtScript/QScriptEngineAgent>
#include <QJSValue>

#include <QDBusContext>
#include <QDBusMessage>

class QQmlComponent;
class QQmlContext;
class QQmlEngine;
class QAction;
class QDBusPendingCallWatcher;
class QGraphicsScene;
class QMenu;
class QMutex;
class QScriptEngine;
class QScriptValue;
class QQuickWindow;
class KConfigGroup;

/// @c true == javascript, @c false == qml
typedef QList< QPair<bool, QPair<QString, QString > > > LoadScriptList;

namespace KWin
{
class AbstractClient;
class ScriptUnloaderAgent;
class QtScriptWorkspaceWrapper;
class X11Client;

class KWIN_EXPORT AbstractScript : public QObject
{
    Q_OBJECT
public:
    AbstractScript(int id, QString scriptName, QString pluginName, QObject *parent = nullptr);
    ~AbstractScript() override;
    QString fileName() const {
        return m_fileName;
    }
    const QString &pluginName() {
        return m_pluginName;
    }

    void printMessage(const QString &message);
    void registerShortcut(QAction *a, QScriptValue callback);
    /**
     * @brief Registers the given @p callback to be invoked whenever the UserActionsMenu is about
     * to be showed. In the callback the script can create a further sub menu or menu entry to be
     * added to the UserActionsMenu.
     *
     * @param callback Script method to execute when the UserActionsMenu is about to be shown.
     * @return void
     * @see actionsForUserActionMenu
     */
    void registerUseractionsMenuCallback(QScriptValue callback);
    /**
     * @brief Creates actions for the UserActionsMenu by invoking the registered callbacks.
     *
     * This method invokes all the callbacks previously registered with registerUseractionsMenuCallback.
     * The Client @p c is passed in as an argument to the invoked method.
     *
     * The invoked method is supposed to return a JavaScript object containing either the menu or
     * menu entry to be added. In case the callback returns a null or undefined or any other invalid
     * value, it is not considered for adding to the menu.
     *
     * The JavaScript object structure for a menu entry looks like the following:
     * @code
     * {
     *     title: "My Menu Entry",
     *     checkable: true,
     *     checked: false,
     *     triggered: function (action) {
     *         // callback when the menu entry is triggered with the QAction as argument
     *     }
     * }
     * @endcode
     *
     * To construct a complete Menu the JavaScript object looks like the following:
     * @code
     * {
     *     title: "My Menu Title",
     *     items: [{...}, {...}, ...] // list of menu entries as described above
     * }
     * @endcode
     *
     * The returned JavaScript object is introspected and for a menu entry a QAction is created,
     * while for a menu a QMenu is created and QActions for the individual entries. Of course it
     * is allowed to have nested structures.
     *
     * All created objects are (grand) children to the passed in @p parent menu, so that they get
     * deleted whenever the menu is destroyed.
     *
     * @param c The Client for which the menu is invoked, passed to the callback
     * @param parent The Parent for the created Menus or Actions
     * @return QList< QAction* > List of QActions obtained from asking the registered callbacks
     * @see registerUseractionsMenuCallback
     */
    QList<QAction*> actionsForUserActionMenu(AbstractClient *c, QMenu *parent);

    KConfigGroup config() const;
    const QHash<QAction*, QScriptValue> &shortcutCallbacks() const {
        return m_shortcutCallbacks;
    }
    QHash<int, QList<QScriptValue > > &screenEdgeCallbacks() {
        return m_screenEdgeCallbacks;
    }

    int registerCallback(QScriptValue value);

public Q_SLOTS:
    Q_SCRIPTABLE void stop();
    Q_SCRIPTABLE virtual void run() = 0;
    void slotPendingDBusCall(QDBusPendingCallWatcher *watcher);

private Q_SLOTS:
    void globalShortcutTriggered();
    bool borderActivated(ElectricBorder edge);
    /**
     * @brief Slot invoked when a menu action is destroyed. Used to remove the action and callback
     * from the map of actions.
     *
     * @param object The destroyed action
     */
    void actionDestroyed(QObject *object);

Q_SIGNALS:
    Q_SCRIPTABLE void print(const QString &text);
    void runningChanged(bool);

protected:
    bool running() const {
        return m_running;
    }
    void setRunning(bool running) {
        if (m_running == running) {
            return;
        }
        m_running = running;
        emit runningChanged(m_running);
    }
    int scriptId() const {
        return m_scriptId;
    }

private:
    /**
     * @brief Parses the @p value to either a QMenu or QAction.
     *
     * @param value The ScriptValue describing either a menu or action
     * @param parent The parent to use for the created menu or action
     * @return QAction* The parsed action or menu action, if parsing fails returns @c null.
     */
    QAction *scriptValueToAction(QScriptValue &value, QMenu *parent);
    /**
     * @brief Creates a new QAction from the provided data and registers it for invoking the
     * @p callback when the action is triggered.
     *
     * The created action is added to the map of actions and callbacks shared with the global
     * shortcuts.
     *
     * @param title The title of the action
     * @param checkable Whether the action is checkable
     * @param checked Whether the checkable action is checked
     * @param callback The callback to invoke when the action is triggered
     * @param parent The parent to be used for the new created action
     * @return QAction* The created action
     */
    QAction *createAction(const QString &title, bool checkable, bool checked, QScriptValue &callback, QMenu *parent);
    /**
     * @brief Parses the @p items and creates a QMenu from it.
     *
     * @param title The title of the Menu.
     * @param items JavaScript Array containing Menu items.
     * @param parent The parent to use for the new created menu
     * @return QAction* The menu action for the new Menu
     */
    QAction *createMenu(const QString &title, QScriptValue &items, QMenu *parent);
    int m_scriptId;
    QString m_fileName;
    QString m_pluginName;
    bool m_running;
    QHash<QAction*, QScriptValue> m_shortcutCallbacks;
    QHash<int, QList<QScriptValue> > m_screenEdgeCallbacks;
    QHash<int, QScriptValue> m_callbacks;
    /**
     * @brief List of registered functions to call when the UserActionsMenu is about to show
     * to add further entries.
     */
    QList<QScriptValue> m_userActionsMenuCallbacks;
};

class Script : public AbstractScript, QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Scripting")
public:

    Script(int id, QString scriptName, QString pluginName, QObject *parent = nullptr);
    ~Script() override;
    QScriptEngine *engine() {
        return m_engine;
    }

    bool registerTouchScreenCallback(int edge, QScriptValue callback);
    bool unregisterTouchScreenCallback(int edge);

public Q_SLOTS:
    Q_SCRIPTABLE void run() override;

Q_SIGNALS:
    Q_SCRIPTABLE void printError(const QString &text);

private Q_SLOTS:
    /**
     * A nice clean way to handle exceptions in scripting.
     * TODO: Log to file, show from notifier..
     */
    void sigException(const QScriptValue &exception);
    /**
     * Callback for when loadScriptFromFile has finished.
     */
    void slotScriptLoadedFromFile();

private:
    void installScriptFunctions(QScriptEngine *engine);
    /**
     * Read the script from file into a byte array.
     * If file cannot be read an empty byte array is returned.
     */
    QByteArray loadScriptFromFile(const QString &fileName);
    QScriptEngine *m_engine;
    QDBusMessage m_invocationContext;
    bool m_starting;
    QScopedPointer<ScriptUnloaderAgent> m_agent;
    QHash<int, QAction*> m_touchScreenEdgeCallbacks;
};

class ScriptUnloaderAgent : public QScriptEngineAgent
{
public:
    explicit ScriptUnloaderAgent(Script *script);
    void scriptUnload(qint64 id) override;

private:
    Script *m_script;
};

class DeclarativeScript : public AbstractScript
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Scripting")
public:
    explicit DeclarativeScript(int id, QString scriptName, QString pluginName, QObject *parent = nullptr);
    ~DeclarativeScript() override;

public Q_SLOTS:
    Q_SCRIPTABLE void run() override;

private Q_SLOTS:
    void createComponent();

private:
    QQmlContext *m_context;
    QQmlComponent *m_component;
};

class JSEngineGlobalMethodsWrapper : public QObject
{
    Q_OBJECT
    Q_ENUMS(ClientAreaOption)
public:
//------------------------------------------------------------------
//enums copy&pasted from kwinglobals.h for exporting

    enum ClientAreaOption {
        ///< geometry where a window will be initially placed after being mapped
        PlacementArea,
        ///< window movement snapping area?  ignore struts
        MovementArea,
        ///< geometry to which a window will be maximized
        MaximizeArea,
        ///< like MaximizeArea, but ignore struts - used e.g. for topmenu
        MaximizeFullArea,
        ///< area for fullscreen windows
        FullScreenArea,
        ///< whole workarea (all screens together)
        WorkArea,
        ///< whole area (all screens together), ignore struts
        FullArea,
        ///< one whole screen, ignore struts
        ScreenArea
    };
    explicit JSEngineGlobalMethodsWrapper(DeclarativeScript *parent);
    ~JSEngineGlobalMethodsWrapper() override;

public Q_SLOTS:
    QVariant readConfig(const QString &key, QVariant defaultValue = QVariant());
    void registerWindow(QQuickWindow *window);
    bool registerShortcut(const QString &name, const QString &text, const QKeySequence& keys, QJSValue function);

private:
    DeclarativeScript *m_script;
};

/**
 * The heart of KWin::Scripting. Infinite power lies beyond
 */
class KWIN_EXPORT Scripting : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Scripting")
private:
    explicit Scripting(QObject *parent);
    QStringList scriptList;
    QList<KWin::AbstractScript*> scripts;
    /**
     * Lock to protect the scripts member variable.
     */
    QScopedPointer<QMutex> m_scriptsLock;

    // Preferably call ONLY at load time
    void runScripts();

public:
    ~Scripting() override;
    Q_SCRIPTABLE Q_INVOKABLE int loadScript(const QString &filePath, const QString &pluginName = QString());
    Q_SCRIPTABLE Q_INVOKABLE int loadDeclarativeScript(const QString &filePath, const QString &pluginName = QString());
    Q_SCRIPTABLE Q_INVOKABLE bool isScriptLoaded(const QString &pluginName) const;
    Q_SCRIPTABLE Q_INVOKABLE bool unloadScript(const QString &pluginName);

    /**
     * @brief Invokes all registered callbacks to add actions to the UserActionsMenu.
     *
     * @param c The Client for which the UserActionsMenu is about to be shown
     * @param parent The parent menu to which to add created child menus and items
     * @return QList< QAction* > List of all actions aggregated from all scripts.
     */
    QList<QAction*> actionsForUserActionMenu(AbstractClient *c, QMenu *parent);

    QQmlEngine *qmlEngine() const;
    QQmlEngine *qmlEngine();
    QQmlContext *declarativeScriptSharedContext() const;
    QQmlContext *declarativeScriptSharedContext();
    QtScriptWorkspaceWrapper *workspaceWrapper() const;

    AbstractScript *findScript(const QString &pluginName) const;

    static Scripting *self();
    static Scripting *create(QObject *parent);

public Q_SLOTS:
    void scriptDestroyed(QObject *object);
    Q_SCRIPTABLE void start();

private Q_SLOTS:
    void slotScriptsQueried();

private:
    void init();
    LoadScriptList queryScriptsToLoad();
    static Scripting *s_self;
    QQmlEngine *m_qmlEngine;
    QQmlContext *m_declarativeScriptSharedContext;
    QtScriptWorkspaceWrapper *m_workspaceWrapper;
};

inline
QQmlEngine *Scripting::qmlEngine() const
{
    return m_qmlEngine;
}

inline
QQmlEngine *Scripting::qmlEngine()
{
    return m_qmlEngine;
}

inline
QQmlContext *Scripting::declarativeScriptSharedContext() const
{
    return m_declarativeScriptSharedContext;
}

inline
QQmlContext *Scripting::declarativeScriptSharedContext()
{
    return m_declarativeScriptSharedContext;
}

inline
QtScriptWorkspaceWrapper *Scripting::workspaceWrapper() const
{
    return m_workspaceWrapper;
}

inline
Scripting *Scripting::self()
{
    return s_self;
}

}
#endif
