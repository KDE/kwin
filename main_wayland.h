/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_MAIN_WAYLAND_H
#define KWIN_MAIN_WAYLAND_H
#include "main.h"
#include <QtCore/private/qeventdispatcher_unix_p.h>
#include <QProcessEnvironment>

class QProcess;

namespace KWin
{

class ApplicationWayland : public Application
{
    Q_OBJECT
public:
    ApplicationWayland(int &argc, char **argv);
    virtual ~ApplicationWayland();

    void setStartXwayland(bool start) {
        m_startXWayland = start;
    }
    void setApplicationsToStart(const QStringList &applications) {
        m_applicationsToStart = applications;
    }
    void setInputMethodServerToStart(const QString &inputMethodServer) {
        m_inputMethodServerToStart = inputMethodServer;
    }
    void setProcessStartupEnvironment(const QProcessEnvironment &environment) {
        m_environment = environment;
    }

    bool notify(QObject *o, QEvent *e) override;

protected:
    void performStartup() override;

private:
    void createBackend();
    void createX11Connection();
    void continueStartupWithScreens();
    void continueStartupWithX();
    void startXwaylandServer();

    bool m_startXWayland = false;
    int m_xcbConnectionFd = -1;
    QStringList m_applicationsToStart;
    QString m_inputMethodServerToStart;
    QProcess *m_xwaylandProcess = nullptr;
    QProcessEnvironment m_environment;
};

class EventDispatcher : public QEventDispatcherUNIX
{
    Q_OBJECT
public:
    explicit EventDispatcher(QObject *parent = nullptr);
    virtual ~EventDispatcher();

    bool processEvents(QEventLoop::ProcessEventsFlags flags) override;
    bool hasPendingEvents() override;
};

}

#endif
