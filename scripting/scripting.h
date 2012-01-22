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

#include <QDir>

class QScriptEngine;
class QScriptValue;

namespace KWin
{
class WorkspaceWrapper;

class Script : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Scripting")
public:

    Script(int id, QString scriptName, QDir dir, QObject *parent = NULL);
    virtual ~Script();
    QString fileName() const {
        return m_scriptFile.fileName();
    }

    void printMessage(const QString &message);

public Q_SLOTS:
    Q_SCRIPTABLE void stop();
    Q_SCRIPTABLE void run();

Q_SIGNALS:
    Q_SCRIPTABLE void print(const QString &text);
    Q_SCRIPTABLE void printError(const QString &text);

private slots:
    /**
      * A nice clean way to handle exceptions in scripting.
      * TODO: Log to file, show from notifier..
      */
    void sigException(const QScriptValue &exception);

private:
    int m_scriptId;
    QScriptEngine *m_engine;
    QDir m_scriptDir;
    QFile m_scriptFile;
    QString m_configFile;
    WorkspaceWrapper *m_workspace;
    bool m_running;
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
    QDir scriptsDir;
    QList<KWin::Script*> scripts;

    // Preferably call ONLY at load time
    void runScripts();

public:
    Scripting(QObject *parent = NULL);
    /**
      * Start running scripts. This was essential to have KWin::Scripting
      * be initialized on stack and also have the option to disable scripting.
      */
    void start();
    ~Scripting();
    Q_SCRIPTABLE Q_INVOKABLE int loadScript(const QString &filePath);

public Q_SLOTS:
    void scriptDestroyed(QObject *object);
};

}
#endif
