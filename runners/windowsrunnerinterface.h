/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <kde@martin-graesslin.com>
    SPDX-FileCopyrightText: 2020 Benjamin Port <benjamin.port@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef WINDOWSRUNNER_H
#define WINDOWSRUNNER_H

#pragma once

#include <QObject>
#include <QDBusContext>
#include <QDBusMessage>
#include <QString>
#include <QDBusArgument>

#include <KRunner/QueryMatch>
#include "dbusutils_p.h"

namespace KWin
{
class VirtualDesktop;
class AbstractClient;

class WindowsRunner : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.WindowsRunner")
public:
    explicit WindowsRunner(QObject *parent = nullptr);
    ~WindowsRunner() override;

    RemoteActions Actions();
    RemoteMatches Match(const QString &searchTerm);
    void Run(const QString &id, const QString &actionId);

private:
    enum WindowsRunnerAction {
        // Windows related actions
        ActivateAction,
        CloseAction,
        MinimizeAction,
        MaximizeAction,
        FullscreenAction,
        ShadeAction,
        KeepAboveAction,
        KeepBelowAction,
        // Desktop related actions
        ActivateDesktopAction
    };

    RemoteMatch desktopMatch(const VirtualDesktop *desktop, const WindowsRunnerAction action = ActivateDesktopAction,  qreal relevance = 1.0) const;
    RemoteMatch windowsMatch(const AbstractClient *client,  const WindowsRunnerAction action = ActivateAction, qreal relevance = 1.0, Plasma::QueryMatch::Type type = Plasma::QueryMatch::ExactMatch) const;
    bool actionSupported(const AbstractClient *client, const WindowsRunnerAction action) const;
};
}

#endif // WINDOWSRUNNER_H
