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

    XwaylandInterface *xwayland() const override;

protected:
    void performStartup() override;

private:
    void continueStartupWithScene();
    void finalizeStartup();
    void startSession();
    void refreshSettings(const KConfigGroup &group, const QByteArrayList &names);

    bool m_startXWayland = false;
    QStringList m_applicationsToStart;
    QString m_inputMethodServerToStart;
    QString m_sessionArgument;

    std::unique_ptr<Xwl::Xwayland> m_xwayland;
    QVector<int> m_xwaylandListenFds;
    QString m_xwaylandDisplay;
    QString m_xwaylandXauthority;
    KConfigWatcher::Ptr m_settingsWatcher;
};

}
