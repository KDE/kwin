/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Rohan Prabhu <rohan@rohanprabhu.com>

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

#include <QScriptClass>
#include <QScriptEngine>
#include <QDir>
#include <QVector>
#include <QSettings>
#include <QVariant>

#include "chelate.h"
#include "workspace.h"
#include "workspaceproxy.h"
#include "s_clientgroup.h"

#include "./../workspace.h"

#include "meta.h"

namespace KWin
{

/** This mostly behaves like a struct. Is used to store
  * the scriptfile, the configfile and the QScriptEngine
  * that will run this script
  */
class Script
    {
    public:
        QScriptEngine* engine;
        QFile scriptFile;
        QString configFile;
        SWrapper::Workspace* workspace;

        Script(QScriptEngine* _engine, QString scriptName, QDir dir) :
            engine(_engine)
            {
            scriptFile.setFileName(dir.filePath(scriptName));
            configFile = (QFileInfo(scriptFile).completeBaseName() + QString(".kwscfg"));
            }

        ~Script()
            {
            delete engine;
            }
    };

/**
  * The heart of KWin::Scripting. Infinite power lies beyond
  */
class Scripting : public QScriptEngine
    {
        Q_OBJECT
    private:
        QStringList scriptList;
        QDir scriptsDir;
        QVector<KWin::Script*> scripts;
        SWrapper::WorkspaceProxy proxy;

        // Preferably call ONLY at load time
        void runScripts();

        // NOTE: Runtime script running is not yet tested.
        // Proceed with caution.
        // An interface to run scripts at runtime
        void runScript(KWin::Script*);

    public slots:
        /**
          * A nice clean way to handle exceptions in scripting.
          * TODO: Log to file, show from notifier..
          */
        void sigException(const QScriptValue&);

    public:
        Scripting();
        /**
          * Start running scripts. This was essential to have KWin::Scripting
          * be initialized on stack and also have the option to disable scripting.
          */
        void start();
        ~Scripting();
    };

}
#endif
