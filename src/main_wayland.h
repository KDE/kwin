/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "main.h"
#include <KConfigWatcher>
#include <QTimer>

#include "utils/filedescriptor.h"

#include <vector>

namespace KWin
{
namespace Xwl
{
class Xwayland;
}

class ApplicationWayland : public Application
{
    Q_OBJECT
public:
    ApplicationWayland(int &argc, char **argv);
    ~ApplicationWayland() override;

#if KWIN_BUILD_X11
    void setStartXwayland(bool start)
    {
        m_startXWayland = start;
    }
    void addXwaylandSocketFileDescriptor(int fd)
    {
        m_xwaylandListenFds << fd;
    }
    void setXwaylandDisplay(const QString &display)
    {
        m_xwaylandDisplay = display;
    }
    void setXwaylandXauthority(const QString &xauthority)
    {
        m_xwaylandXauthority = xauthority;
    }
    void addExtraXWaylandEnvrionmentVariable(const QString &variable, const QString &value)
    {
        m_xwaylandExtraEnvironment.insert(variable, value);
    }
    void passFdToXwayland(FileDescriptor &&fd)
    {
        m_xwaylandFds.push_back(std::move(fd));
    }
    XwaylandInterface *xwayland() const override;

    pid_t xwaylandPid() const override;
#endif
    void setApplicationsToStart(const QStringList &applications)
    {
        m_applicationsToStart = applications;
    }
    void setInputMethodServerToStart(const QString &inputMethodServer)
    {
        m_inputMethodServerToStart = inputMethodServer;
    }
    void setSessionArgument(const QString &session)
    {
        m_sessionArgument = session;
    }

protected:
    void performStartup() override;

private:
    void startSession();
    void refreshSettings(const KConfigGroup &group, const QByteArrayList &names);

    QStringList m_applicationsToStart;
    QString m_inputMethodServerToStart;
    QString m_sessionArgument;

#if KWIN_BUILD_X11
    bool m_startXWayland = false;
    std::unique_ptr<Xwl::Xwayland> m_xwayland;
    QList<int> m_xwaylandListenFds;
    QString m_xwaylandDisplay;
    QString m_xwaylandXauthority;
    QMap<QString, QString> m_xwaylandExtraEnvironment;
    std::vector<FileDescriptor> m_xwaylandFds;
#endif
    KConfigWatcher::Ptr m_settingsWatcher;
};

}
