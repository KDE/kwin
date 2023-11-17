/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Rohan Prabhu <rohan@rohanprabhu.com>
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/globals.h"

#include <QHash>
#include <QJSEngine>
#include <QJSValue>
#include <QStringList>
#include <QTimer>

#include <QDBusContext>
#include <QDBusMessage>

class QQmlComponent;
class QQmlContext;
class QQmlEngine;
class QAction;
class QMenu;
class QRecursiveMutex;
class QQuickWindow;
class KConfigGroup;

/// @c true == javascript, @c false == qml
typedef QList<QPair<bool, QPair<QString, QString>>> LoadScriptList;

namespace KWin
{
class Window;
class QtScriptWorkspaceWrapper;

class KWIN_EXPORT AbstractScript : public QObject
{
    Q_OBJECT
public:
    AbstractScript(int id, QString scriptName, QString pluginName, QObject *parent = nullptr);
    ~AbstractScript() override;
    int scriptId() const
    {
        return m_scriptId;
    }
    QString fileName() const
    {
        return m_fileName;
    }
    const QString &pluginName()
    {
        return m_pluginName;
    }
    bool running() const
    {
        return m_running;
    }

    KConfigGroup config() const;

public Q_SLOTS:
    void stop();
    virtual void run() = 0;

Q_SIGNALS:
    void runningChanged(bool);

protected:
    void setRunning(bool running)
    {
        if (m_running == running) {
            return;
        }
        m_running = running;
        Q_EMIT runningChanged(m_running);
    }

private:
    int m_scriptId;
    QString m_fileName;
    QString m_pluginName;
    bool m_running;
};

/**
 * In order to be able to construct QTimer objects in javascript, the constructor
 * must be declared with Q_INVOKABLE.
 */
class ScriptTimer : public QTimer
{
    Q_OBJECT

public:
    Q_INVOKABLE ScriptTimer(QObject *parent = nullptr);
};

class Script : public AbstractScript, QDBusContext
{
    Q_OBJECT
public:
    Script(int id, QString scriptName, QString pluginName, QObject *parent = nullptr);
    virtual ~Script();

    Q_INVOKABLE QVariant readConfig(const QString &key, const QVariant &defaultValue = QVariant());

    Q_INVOKABLE void callDBus(const QString &service, const QString &path,
                              const QString &interface, const QString &method,
                              const QJSValue &arg1 = QJSValue(),
                              const QJSValue &arg2 = QJSValue(),
                              const QJSValue &arg3 = QJSValue(),
                              const QJSValue &arg4 = QJSValue(),
                              const QJSValue &arg5 = QJSValue(),
                              const QJSValue &arg6 = QJSValue(),
                              const QJSValue &arg7 = QJSValue(),
                              const QJSValue &arg8 = QJSValue(),
                              const QJSValue &arg9 = QJSValue());

    Q_INVOKABLE bool registerShortcut(const QString &objectName, const QString &text,
                                      const QString &keySequence, const QJSValue &callback);

    Q_INVOKABLE bool registerScreenEdge(int edge, const QJSValue &callback);
    Q_INVOKABLE bool unregisterScreenEdge(int edge);

    Q_INVOKABLE bool registerTouchScreenEdge(int edge, const QJSValue &callback);
    Q_INVOKABLE bool unregisterTouchScreenEdge(int edge);

    /**
     * @brief Registers the given @p callback to be invoked whenever the UserActionsMenu is about
     * to be showed. In the callback the script can create a further sub menu or menu entry to be
     * added to the UserActionsMenu.
     *
     * @param callback Script method to execute when the UserActionsMenu is about to be shown.
     * @return void
     * @see actionsForUserActionMenu
     */
    Q_INVOKABLE void registerUserActionsMenu(const QJSValue &callback);

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
    QList<QAction *> actionsForUserActionMenu(Window *client, QMenu *parent);

public Q_SLOTS:
    void run() override;

private Q_SLOTS:
    /**
     * Callback for when loadScriptFromFile has finished.
     */
    void slotScriptLoadedFromFile();

    /**
     * Called when any reserve screen edge is triggered.
     */
    bool slotBorderActivated(ElectricBorder border);

private:
    /**
     * Read the script from file into a byte array.
     * If file cannot be read an empty byte array is returned.
     */
    QByteArray loadScriptFromFile(const QString &fileName);

    /**
     * @brief Parses the @p value to either a QMenu or QAction.
     *
     * @param value The ScriptValue describing either a menu or action
     * @param parent The parent to use for the created menu or action
     * @return QAction* The parsed action or menu action, if parsing fails returns @c null.
     */
    QAction *scriptValueToAction(const QJSValue &value, QMenu *parent);

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
    QAction *createAction(const QString &title, const QJSValue &item, QMenu *parent);

    /**
     * @brief Parses the @p items and creates a QMenu from it.
     *
     * @param title The title of the Menu.
     * @param items JavaScript Array containing Menu items.
     * @param parent The parent to use for the new created menu
     * @return QAction* The menu action for the new Menu
     */
    QAction *createMenu(const QString &title, const QJSValue &items, QMenu *parent);

    QJSEngine *m_engine;
    QDBusMessage m_invocationContext;
    bool m_starting;
    QHash<int, QJSValueList> m_screenEdgeCallbacks;
    QHash<int, QAction *> m_touchScreenEdgeCallbacks;
    QJSValueList m_userActionsMenuCallbacks;
};

class DeclarativeScript : public AbstractScript
{
    Q_OBJECT
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
public:
    //------------------------------------------------------------------
    // enums copy&pasted from kwinglobals.h for exporting

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
    Q_ENUM(ClientAreaOption)
    explicit JSEngineGlobalMethodsWrapper(DeclarativeScript *parent);
    ~JSEngineGlobalMethodsWrapper() override;

public Q_SLOTS:
    QVariant readConfig(const QString &key, QVariant defaultValue = QVariant());

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
    QList<KWin::AbstractScript *> scripts;
    /**
     * Lock to protect the scripts member variable.
     */
    std::unique_ptr<QRecursiveMutex> m_scriptsLock;

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
    QList<QAction *> actionsForUserActionMenu(Window *c, QMenu *parent);

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

inline QQmlEngine *Scripting::qmlEngine() const
{
    return m_qmlEngine;
}

inline QQmlEngine *Scripting::qmlEngine()
{
    return m_qmlEngine;
}

inline QQmlContext *Scripting::declarativeScriptSharedContext() const
{
    return m_declarativeScriptSharedContext;
}

inline QQmlContext *Scripting::declarativeScriptSharedContext()
{
    return m_declarativeScriptSharedContext;
}

inline QtScriptWorkspaceWrapper *Scripting::workspaceWrapper() const
{
    return m_workspaceWrapper;
}

inline Scripting *Scripting::self()
{
    return s_self;
}

}
