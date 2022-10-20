/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <kde@martin-graesslin.com>
    SPDX-FileCopyrightText: 2020 Benjamin Port <benjamin.port@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#pragma once

#include "dbusutils_p.h"
#include "plugin.h"

#include <KRunner/QueryMatch>

#include <QDBusArgument>
#include <QDBusContext>
#include <QDBusMessage>
#include <QObject>
#include <QString>

namespace KWin
{
class VirtualDesktop;
class Window;

class WindowsRunner : public Plugin, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.WindowsRunner")
public:
    explicit WindowsRunner();
    ~WindowsRunner() override;

    RemoteActions Actions();
    RemoteMatches Match(const QString &searchTerm);
    void Run(const QString &id, const QString &actionId);

private:
    enum WindowsRunnerAction {
        // Windows and desktop related actions
        ActivateAction,
        // Windows related actions
        CloseAction,
        MinimizeAction,
        MaximizeAction,
        FullscreenAction,
        ShadeAction,
        KeepAboveAction,
        KeepBelowAction,
        PinAction,
        MoveAction,
        // Desktop related actions
        AddDesktopAction,
        RemoveDesktopAction
    };

    enum MatchCategory {
        DesktopMatch,
        WindowMatch,
        WindowsDesktopMatch
    };

    RemoteMatch desktopMatch(const VirtualDesktop *desktop, const WindowsRunnerAction action = ActivateAction, qreal relevance = 1.0, Plasma::QueryMatch::Type type = Plasma::QueryMatch::ExactMatch) const;
    RemoteMatch windowMatch(const Window *window, const WindowsRunnerAction action = ActivateAction, const VirtualDesktop *destination = nullptr, qreal relevance = 1.0, Plasma::QueryMatch::Type type = Plasma::QueryMatch::ExactMatch) const;
    RemoteMatch windowsDesktopMatch(const VirtualDesktop *desktop, const WindowsRunnerAction action, const VirtualDesktop *destination = nullptr, qreal relevance = 1.0, Plasma::QueryMatch::Type type = Plasma::QueryMatch::ExactMatch) const;
    bool actionSupported(const Window *window, const WindowsRunnerAction action) const;
};
}
