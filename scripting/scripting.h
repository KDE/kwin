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

#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QStringList>

class QAction;
class QDeclarativeView;
class QMutex;
class QScriptEngine;
class QScriptValue;
class KConfigGroup;

/// @c true == javascript, @c false == qml
typedef QList< QPair<bool, QPair<QString, QString > > > LoadScriptList;

namespace KWin
{
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

    KConfigGroup config() const;
    const QHash<QAction*, QScriptValue> &shortcutCallbacks() const {
        return m_shortcutCallbacks;
    }

public Q_SLOTS:
    Q_SCRIPTABLE void stop();
    Q_SCRIPTABLE virtual void run() = 0;

private Q_SLOTS:
    void globalShortcutTriggered();

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
    int m_scriptId;
    QFile m_scriptFile;
    QString m_pluginName;
    bool m_running;
    WorkspaceWrapper *m_workspace;
    QHash<QAction*, QScriptValue> m_shortcutCallbacks;
};

class Script : public AbstractScript
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Scripting")
public:

    Script(int id, QString scriptName, QString pluginName, QObject *parent = NULL);
    virtual ~Script();

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
    QDeclarativeView *m_view;
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

public Q_SLOTS:
    void scriptDestroyed(QObject *object);
    void start();

private Q_SLOTS:
    void slotScriptsQueried();

private:
    LoadScriptList queryScriptsToLoad();
};

}
#endif
