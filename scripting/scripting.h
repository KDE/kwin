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

#ifndef KWIN_SCRIPTING_H
#define KWIN_SCRIPTING_H

#include <kwinglobals.h>
#include <kservice.h>

#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QStringList>
#include <QtScript/QScriptEngineAgent>

class QAction;
class QDeclarativeView;
class QDBusPendingCallWatcher;
class QMenu;
class QMutex;
class QScriptEngine;
class QScriptValue;
class KConfigGroup;

/// @c true == javascript, @c false == qml
typedef QList< QPair<bool, QPair<QString, QString > > > LoadScriptList;

namespace KWin
{
class Client;
class ScriptUnloaderAgent;
class WorkspaceWrapper;

class AbstractScript : public QObject
{
    Q_OBJECT
public:
    AbstractScript(int id, QString scriptName, QString pluginName, QObject *parent = NULL);
    ~AbstractScript();
    QString fileName() const {
        return m_scriptFile.fileName();
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
     **/
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
     **/
    QList<QAction*> actionsForUserActionMenu(Client *c, QMenu *parent);

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
    void borderActivated(ElectricBorder edge);
    /**
     * @brief Slot invoked when a menu action is destroyed. Used to remove the action and callback
     * from the map of actions.
     *
     * @param object The destroyed action
     **/
    void actionDestroyed(QObject *object);

Q_SIGNALS:
    Q_SCRIPTABLE void print(const QString &text);

protected:
    QFile &scriptFile() {
        return m_scriptFile;
    }
    bool running() const {
        return m_running;
    }
    void setRunning(bool running) {
        m_running = running;
    }
    int scriptId() const {
        return m_scriptId;
    }

    WorkspaceWrapper *workspace() {
        return m_workspace;
    }

    void installScriptFunctions(QScriptEngine *engine);

private:
    /**
     * @brief Parses the @p value to either a QMenu or QAction.
     *
     * @param value The ScriptValue describing either a menu or action
     * @param parent The parent to use for the created menu or action
     * @return QAction* The parsed action or menu action, if parsing fails returns @c null.
     **/
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
     **/
    QAction *createAction(const QString &title, bool checkable, bool checked, QScriptValue &callback, QMenu *parent);
    /**
     * @brief Parses the @p items and creates a QMenu from it.
     *
     * @param title The title of the Menu.
     * @param items JavaScript Array containing Menu items.
     * @param parent The parent to use for the new created menu
     * @return QAction* The menu action for the new Menu
     **/
    QAction *createMenu(const QString &title, QScriptValue &items, QMenu *parent);
    int m_scriptId;
    QFile m_scriptFile;
    QString m_pluginName;
    bool m_running;
    WorkspaceWrapper *m_workspace;
    QHash<QAction*, QScriptValue> m_shortcutCallbacks;
    QHash<int, QList<QScriptValue> > m_screenEdgeCallbacks;
    QHash<int, QScriptValue> m_callbacks;
    /**
     * @brief List of registered functions to call when the UserActionsMenu is about to show
     * to add further entries.
     **/
    QList<QScriptValue> m_userActionsMenuCallbacks;
};

class Script : public AbstractScript
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Scripting")
public:

    Script(int id, QString scriptName, QString pluginName, QObject *parent = NULL);
    virtual ~Script();
    QScriptEngine *engine() {
        return m_engine;
    }

public Q_SLOTS:
    Q_SCRIPTABLE void run();

Q_SIGNALS:
    Q_SCRIPTABLE void printError(const QString &text);

private slots:
    /**
      * A nice clean way to handle exceptions in scripting.
      * TODO: Log to file, show from notifier..
      */
    void sigException(const QScriptValue &exception);
    /**
     * Callback for when loadScriptFromFile has finished.
     **/
    void slotScriptLoadedFromFile();

private:
    /**
     * Read the script from file into a byte array.
     * If file cannot be read an empty byte array is returned.
     **/
    QByteArray loadScriptFromFile();
    QScriptEngine *m_engine;
    bool m_starting;
    QScopedPointer<ScriptUnloaderAgent> m_agent;
};

class ScriptUnloaderAgent : public QScriptEngineAgent
{
public:
    ScriptUnloaderAgent(Script *script);
    virtual void scriptUnload(qint64 id);

private:
    Script *m_script;
};

class DeclarativeScript : public AbstractScript
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Scripting")
public:
    explicit DeclarativeScript(int id, QString scriptName, QString pluginName, QObject *parent = 0);
    virtual ~DeclarativeScript();

public Q_SLOTS:
    Q_SCRIPTABLE void run();

private:
    QScopedPointer<QDeclarativeView> m_view;
};

/**
  * The heart of KWin::Scripting. Infinite power lies beyond
  */
class Scripting : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Scripting")
private:
    QStringList scriptList;
    QList<KWin::AbstractScript*> scripts;
    /**
     * Lock to protect the scripts member variable.
     **/
    QScopedPointer<QMutex> m_scriptsLock;

    // Preferably call ONLY at load time
    void runScripts();

public:
    Scripting(QObject *parent = NULL);
    ~Scripting();
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
     **/
    QList<QAction*> actionsForUserActionMenu(Client *c, QMenu *parent);

public Q_SLOTS:
    void scriptDestroyed(QObject *object);
    Q_SCRIPTABLE void start();

private Q_SLOTS:
    void slotScriptsQueried();

private:
    LoadScriptList queryScriptsToLoad(QMap<QString,QString> &pluginStates, KService::List &);
};

}
#endif
