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

#if KWIN_BUILD_X11
    void setStartXwayland(bool start)
    {
        m_startXWayland = start; // this and the arg can now die
        m_xwayland = std::make_unique<Xwl::Xwayland>(this);
    }
    XwaylandInterface *xwayland() const override;
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

    std::unique_ptr<Xwl::Xwayland> m_xwayland;

protected:
    void performStartup() override;

private:
    void continueStartupWithScene();
    void startSession();
    void refreshSettings(const KConfigGroup &group, const QByteArrayList &names);

    QStringList m_applicationsToStart;
    QString m_inputMethodServerToStart;
    QString m_sessionArgument;

#if KWIN_BUILD_X11
    bool m_startXWayland = false;
#endif
    KConfigWatcher::Ptr m_settingsWatcher;
};

}
