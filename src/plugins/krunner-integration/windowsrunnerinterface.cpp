/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <kde@martin-graesslin.com>
    SPDX-FileCopyrightText: 2020 Benjamin Port <benjamin.port@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowsrunnerinterface.h"

#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"

#include "krunner1adaptor.h"
#include <KLocalizedString>
#include <qchar.h>
#include <qstringliteral.h>
#include <xcb/xproto.h>

namespace KWin
{

WindowsRunner::WindowsRunner()
{
    new Krunner1Adaptor(this);
    qDBusRegisterMetaType<RemoteMatch>();
    qDBusRegisterMetaType<RemoteMatches>();
    qDBusRegisterMetaType<RemoteAction>();
    qDBusRegisterMetaType<RemoteActions>();
    qDBusRegisterMetaType<RemoteImage>();
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/WindowsRunner"), this);
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.KWin"));
}

WindowsRunner::~WindowsRunner() = default;

RemoteActions WindowsRunner::Actions()
{
    RemoteActions actions;
    return actions;
}

RemoteMatches WindowsRunner::Match(const QString &searchTerm)
{
    RemoteMatches matches;

    const VirtualDesktopManager *vds = VirtualDesktopManager::self();

    auto term = searchTerm;
    WindowsRunnerAction action = ActivateAction;
    if (term.endsWith(i18nc("Note this is a KRunner keyword", "activate"), Qt::CaseInsensitive)) {
        action = ActivateAction;
        term = term.left(term.lastIndexOf(i18nc("Note this is a KRunner keyword", "activate")) - 1);
    } else if (term.endsWith(i18nc("Note this is a KRunner keyword", "close"), Qt::CaseInsensitive)) {
        action = CloseAction;
        term = term.left(term.lastIndexOf(i18nc("Note this is a KRunner keyword", "close")) - 1);
    } else if (term.endsWith(i18nc("Note this is a KRunner keyword", "min"), Qt::CaseInsensitive)) {
        action = MinimizeAction;
        term = term.left(term.lastIndexOf(i18nc("Note this is a KRunner keyword", "min")) - 1);
    } else if (term.endsWith(i18nc("Note this is a KRunner keyword", "minimize"), Qt::CaseInsensitive)) {
        action = MinimizeAction;
        term = term.left(term.lastIndexOf(i18nc("Note this is a KRunner keyword", "minimize")) - 1);
    } else if (term.endsWith(i18nc("Note this is a KRunner keyword", "max"), Qt::CaseInsensitive)) {
        action = MaximizeAction;
        term = term.left(term.lastIndexOf(i18nc("Note this is a KRunner keyword", "max")) - 1);
    } else if (term.endsWith(i18nc("Note this is a KRunner keyword", "maximize"), Qt::CaseInsensitive)) {
        action = MaximizeAction;
        term = term.left(term.lastIndexOf(i18nc("Note this is a KRunner keyword", "maximize")) - 1);
    } else if (term.endsWith(i18nc("Note this is a KRunner keyword", "fullscreen"), Qt::CaseInsensitive)) {
        action = FullscreenAction;
        term = term.left(term.lastIndexOf(i18nc("Note this is a KRunner keyword", "fullscreen")) - 1);
    } else if (term.endsWith(i18nc("Note this is a KRunner keyword", "shade"), Qt::CaseInsensitive)) {
        action = ShadeAction;
        term = term.left(term.lastIndexOf(i18nc("Note this is a KRunner keyword", "shade")) - 1);
    } else if (term.endsWith(i18nc("Note this is a KRunner keyword", "keep above"), Qt::CaseInsensitive)) {
        action = KeepAboveAction;
        term = term.left(term.lastIndexOf(i18nc("Note this is a KRunner keyword", "keep above")) - 1);
    } else if (term.endsWith(i18nc("Note this is a KRunner keyword", "keep below"), Qt::CaseInsensitive)) {
        action = KeepBelowAction;
        term = term.left(term.lastIndexOf(i18nc("Note this is a KRunner keyword", "keep below")) - 1);
    } else if (term.endsWith(i18nc("Note this is a KRunner keyword", "pin"), Qt::CaseInsensitive)) {
        action = PinAction;
        term = term.left(term.lastIndexOf(i18nc("Note this is a KRunner keyword", "pin")) - 1);
    } else if (term.endsWith(i18nc("Note this is a KRunner keyword", "desktop"), Qt::CaseInsensitive)) {
        action = MoveAction;
        term = term.left(term.lastIndexOf(i18nc("Note this is a KRunner keyword", "desktop")) - 1);
    }

    // keyword match: when term starts with "window" we list all windows
    // the list can be restricted to windows matching a given name, class, role or desktop
    if (term.startsWith(i18nc("Note this is a KRunner keyword", "window"), Qt::CaseInsensitive) && !term.startsWith(i18nc("Note this is a KRunner keyword", "windows"), Qt::CaseInsensitive)) {
        const QStringList keywords = term.split(QLatin1Char(' '));
        QString windowName;
        QString windowAppName;
        VirtualDesktop *targetDesktop = nullptr;
        QVariant desktopId;
        for (const QString &keyword : keywords) {
            if (keyword.endsWith(QLatin1Char('='))) {
                continue;
            }
            if (keyword.startsWith(i18nc("Note this is a KRunner keyword", "name") + QStringLiteral("="), Qt::CaseInsensitive)) {
                windowName = keyword.split(QStringLiteral("="))[1];
            } else if (keyword.startsWith(i18nc("Note this is a KRunner keyword", "appname") + QStringLiteral("="), Qt::CaseInsensitive)) {
                windowAppName = keyword.split(QStringLiteral("="))[1];
            } else if (keyword.startsWith(i18nc("Note this is a KRunner keyword", "desktop") + QStringLiteral("="), Qt::CaseInsensitive)) {
                desktopId = keyword.split(QStringLiteral("="))[1];
                for (const auto desktop : vds->desktops()) {
                    if (desktop->name().contains(desktopId.toString(), Qt::CaseInsensitive) || desktop->x11DesktopNumber() == desktopId.toUInt()) {
                        targetDesktop = desktop;
                    }
                }
            } else {
                // not a keyword - use as name if name is unused, but another option is set
                if (windowName.isEmpty() && !keyword.contains(QLatin1Char('=')) && (!windowAppName.isEmpty() || targetDesktop)) {
                    windowName = keyword;
                }
            }
        }

        for (const Window *window : Workspace::self()->allClientList()) {
            if (!(window->isNormalWindow() || (window->isDesktop() && window->isOnActiveOutput()))) {
                continue;
            }
            const QString appName = window->resourceClass();
            const QString name = window->caption();
            if (!windowName.isEmpty() && !name.startsWith(windowName, Qt::CaseInsensitive)) {
                continue;
            }
            if (!windowAppName.isEmpty() && !appName.contains(windowAppName, Qt::CaseInsensitive)) {
                continue;
            }
            if (!window->isOnCurrentDesktop()) {
                continue;
            }

            if (targetDesktop && !window->desktops().contains(targetDesktop) && !window->isOnAllDesktops()) {
                continue;
            }
            // check for windows when no keywords were used
            // check the name and app name for containing the query without the keyword
            if (windowName.isEmpty() && windowAppName.isEmpty() && !targetDesktop) {
                const QString &test = term.mid(keywords[0].length() + 1);
                if (!name.contains(test, Qt::CaseInsensitive) && !appName.contains(test, Qt::CaseInsensitive)) {
                    continue;
                }
            }
            // blacklisted everything else: we have a match
            if (actionSupported(window, action)) {
                if (action != MoveAction) {
                    matches << windowsMatch(window, action);
                } else {
                    for (auto *desktop : vds->desktops()) {
                        if (!window->isOnDesktop(desktop)) {
                            matches << workspaceMatch(window, desktop, MoveAction);
                        }
                    }
                    matches << workspaceMatch(window, nullptr, MoveAction);
                }
            }
        }

        if (!matches.isEmpty()) {
            // the window keyword found matches - do not process other syntax possibilities
            return matches;
        }
    }

    bool desktopAdded = false;
    // check for desktop keyword
    if (term.startsWith(i18nc("Note this is a KRunner keyword", "desktop"), Qt::CaseInsensitive)) {
        const QStringList parts = term.split(QLatin1Char(' '));
        if (parts.size() == 1) {
            // only keyword - list all desktops
            for (auto desktop : vds->desktops()) {
                matches << desktopMatch(desktop);
                desktopAdded = true;
            }
        }
    }

    // check for matching desktops by name
    for (const Window *window : Workspace::self()->allClientList()) {
        if (!(window->isNormalWindow() || (window->isDesktop() && window->isOnActiveOutput()))) {
            continue;
        }
        const QString appName = window->resourceClass();
        const QString name = window->caption();
        if (!window->isOnCurrentDesktop())
            continue;
        if (name.startsWith(term, Qt::CaseInsensitive) || appName.startsWith(term, Qt::CaseInsensitive)) {
            matches << windowsMatch(window, action, 0.8, Plasma::QueryMatch::ExactMatch);
        } else if ((name.contains(term, Qt::CaseInsensitive) || appName.contains(term, Qt::CaseInsensitive)) && actionSupported(window, action)) {
            matches << windowsMatch(window, action, 0.7, Plasma::QueryMatch::PossibleMatch);
        }
    }

    for (auto *desktop : vds->desktops()) {
        if (desktop->name().contains(term, Qt::CaseInsensitive)) {
            if (!desktopAdded && desktop != vds->currentDesktop()) {
                matches << desktopMatch(desktop, ActivateDesktopAction, 0.8);
                matches << desktopMatch(desktop, MoveAllAction, 0.5, Plasma::QueryMatch::PossibleMatch);
            }
            // search for windows on desktop and list them with less relevance
            for (const Window *window : Workspace::self()->allClientList()) {
                if (!window->isNormalWindow()) {
                    continue;
                }
                if (window->isOnCurrentDesktop() && (window->desktops().contains(desktop) || window->isOnAllDesktops()) && actionSupported(window, action)) {
                    matches << windowsMatch(window, action, 0.5, Plasma::QueryMatch::PossibleMatch);
                }
            }
        }
    }

    // add move all and close all actions
    if (term.startsWith(i18nc("Note this is a KRunner keyword", "windows"))) {
        if (term.endsWith(i18nc("Note this is a KRunner keyword", "desktop"), Qt::CaseInsensitive)) {
            for (auto *desktop : vds->desktops()) {
                if (desktop != vds->currentDesktop()) {
                    matches << workspaceMatch(nullptr, desktop, MoveAllAction);
                }
            }
            matches << workspaceMatch(nullptr, nullptr, MoveAllAction);
        } else if (term.endsWith(i18nc("Note this is a KRunner keyword", "close"), Qt::CaseInsensitive)) {
            matches << workspaceMatch(nullptr, nullptr, CloseAllAction);
        }
    }

    return matches;
}

void WindowsRunner::Run(const QString &id, const QString &actionId)
{
    Q_UNUSED(actionId)
    VirtualDesktopManager *vds = VirtualDesktopManager::self();

    // Split id to get actionId and realId. We don't use actionId because our actions list is not constant
    const QStringList parts = id.split(QLatin1Char('_'));
    auto action = WindowsRunnerAction(parts[0].toInt());
    const auto window = workspace()->findToplevel(QUuid::fromString(parts[1]));
    if (!parts[1].isEmpty() && (!window || !window->isClient())) {
        return;
    }
    const auto desktop = vds->desktopForId(parts[2]);
    if (!parts[2].isEmpty() && !desktop) {
        return;
    }

    switch (action) {
    case ActivateAction:
        workspace()->activateWindow(window);
        break;
    case CloseAction:
        window->closeWindow();
        break;
    case MinimizeAction:
        window->setMinimized(!window->isMinimized());
        break;
    case MaximizeAction:
        window->setMaximize(window->maximizeMode() == MaximizeRestore, window->maximizeMode() == MaximizeRestore);
        break;
    case FullscreenAction:
        window->setFullScreen(!window->isFullScreen());
        break;
    case ShadeAction:
        window->toggleShade();
        break;
    case KeepAboveAction:
        window->setKeepAbove(!window->keepAbove());
        break;
    case KeepBelowAction:
        window->setKeepBelow(!window->keepBelow());
        break;
    case PinAction:
        window->setOnAllDesktops(!window->isOnAllDesktops());
        break;
    case ActivateDesktopAction:
        vds->setCurrent(desktop);
        break;
    case MoveAction: {
        const auto targetDesktop = desktop ? desktop : vds->createVirtualDesktop(vds->count());
        window->setDesktops({targetDesktop});
        break;
    }
    case MoveAllAction: {
        const auto targetDesktop = desktop ? desktop : vds->createVirtualDesktop(vds->count());
        for (Window *w : workspace()->allClientList()) {
            if (w->isOnCurrentDesktop() && actionSupported(w, MoveAction)) {
                window->setDesktops({targetDesktop});
            }
        }
    }
    case CloseAllAction:
        for (Window *w : workspace()->allClientList()) {
            if (w->isOnCurrentDesktop() && actionSupported(w, CloseAction)) {
                w->closeWindow();
            }
        }
        break;
    }
}

RemoteMatch WindowsRunner::desktopMatch(const VirtualDesktop *desktop, const WindowsRunnerAction action, qreal relevance, Plasma::QueryMatch::Type type) const
{
    RemoteMatch match;
    match.id = QString::number(action) + QLatin1Char('_') + QLatin1Char('_') + (desktop ? desktop->id() : "");
    match.type = Plasma::QueryMatch::ExactMatch;
    match.iconName = QStringLiteral("user-desktop");
    match.relevance = relevance;
    match.type = type;

    QVariantMap properties;

    switch (action) {
    case ActivateDesktopAction:
        match.text = desktop->name();
        properties[QStringLiteral("subtext")] = i18n("Switch to desktop %1", desktop->name());
        break;
    case MoveAllAction:
        if (desktop) {
            match.text = i18n("Move all to %1", desktop->name());
            properties[QStringLiteral("subtext")] = i18n("Move all windows on current desktop to desktop %1", desktop->name());
        } else {
            match.text = i18n("Move all to new desktop");
            properties[QStringLiteral("subtext")] = i18n("Move all windows on current desktop to new desktop");
        }
        break;
    default:
        break;
    }
    match.properties = properties;
    return match;
}

RemoteMatch WindowsRunner::windowsMatch(const Window *window, const WindowsRunnerAction action, qreal relevance, Plasma::QueryMatch::Type type) const
{
    RemoteMatch match;
    match.id = QString::number((int)action) + QLatin1Char('_') + window->internalId().toString() + QLatin1Char('_');
    match.text = window->caption();
    match.iconName = window->icon().name();
    match.relevance = relevance;
    match.type = type;
    QVariantMap properties;

    const QVector<VirtualDesktop *> desktops = window->desktops();
    bool allDesktops = window->isOnAllDesktops();

    const auto vds = VirtualDesktopManager::self();
    const VirtualDesktop *targetDesktop = vds->currentDesktop();
    // Show on current desktop unless window is only attached to other desktop, in this case don't return the match
    if (!allDesktops && !window->isOnCurrentDesktop() && !desktops.isEmpty()) {
        targetDesktop = desktops.first();
    }

    // When there is no icon name, send a pixmap along instead
    if (match.iconName.isEmpty()) {
        QImage convertedImage = window->icon().pixmap(QSize(64, 64)).toImage().convertToFormat(QImage::Format_RGBA8888);
        RemoteImage remoteImage{
            convertedImage.width(),
            convertedImage.height(),
            static_cast<int>(convertedImage.bytesPerLine()),
            true, // hasAlpha
            8, // bitsPerSample
            4, // channels
            QByteArray(reinterpret_cast<const char *>(convertedImage.constBits()), convertedImage.sizeInBytes())};
        properties.insert(QStringLiteral("icon-data"), QVariant::fromValue(remoteImage));
    }

    const QString desktopName = targetDesktop->name();
    switch (action) {
    case CloseAction:
        properties[QStringLiteral("subtext")] = i18n("Close running window on %1", desktopName);
        break;
    case MinimizeAction:
        properties[QStringLiteral("subtext")] = i18n("(Un)minimize running window on %1", desktopName);
        break;
    case MaximizeAction:
        properties[QStringLiteral("subtext")] = i18n("Maximize/restore running window on %1", desktopName);
        break;
    case FullscreenAction:
        properties[QStringLiteral("subtext")] = i18n("Toggle fullscreen for running window on %1", desktopName);
        break;
    case ShadeAction:
        properties[QStringLiteral("subtext")] = i18n("(Un)shade running window on %1", desktopName);
        break;
    case KeepAboveAction:
        properties[QStringLiteral("subtext")] = i18n("Toggle keep above for running window on %1", desktopName);
        break;
    case KeepBelowAction:
        properties[QStringLiteral("subtext")] = i18n("Toggle keep below running window on %1", desktopName);
        break;
    case PinAction:
        properties[QStringLiteral("subtext")] = i18n("Toggle pin to all desktops running window on %1", desktopName);
        break;
    case ActivateAction:
    default:
        properties[QStringLiteral("subtext")] = i18n("Activate running window on %1", desktopName);
        break;
    }
    match.properties = properties;
    return match;
}

RemoteMatch WindowsRunner::workspaceMatch(const Window *window, const VirtualDesktop *desktop, const WindowsRunnerAction action, const qreal relevance, Plasma::QueryMatch::Type type) const
{
    RemoteMatch match;
    match.type = type;
    match.id = QString::number((int)action) + QLatin1Char('_') + (window ? window->internalId().toString() : "") + QLatin1Char('_') + (desktop ? desktop->id() : "");
    match.relevance = relevance;
    QVariantMap properties;

    const VirtualDesktopManager *vds = VirtualDesktopManager::self();
    const QString currentDesktopName = window && window->isOnCurrentDesktop() && !window->isOnAllDesktops() && !window->desktops().isEmpty() ? window->desktops().first()->name() : vds->currentDesktop()->name();
    const QString windowName = window ? window->caption() : i18n("All windows");
    const QString desktopName = desktop ? desktop->name() : i18n("new desktop");

    switch (action) {
    case MoveAction: {
        match.text = windowName + i18nc("to", "move window to desktop") + desktopName;
        properties[QStringLiteral("subtext")] = i18n("Move running window on %1 to %2", currentDesktopName, desktopName);
        match.iconName = window->icon().name();
        break;
    }
    case MoveAllAction: {
        match.text = windowName + i18nc("to", "move window to desktop") + desktopName;
        properties[QStringLiteral("subtext")] = i18n("Move all running windows on %1 to %2", currentDesktopName, desktopName);
        match.iconName = QStringLiteral("preferences-system-windows-actions");
        break;
    }
    case CloseAllAction: {
        match.text = windowName;
        properties[QStringLiteral("subtext")] = i18n("Close all running windows on %1", currentDesktopName);
        match.iconName = QStringLiteral("preferences-system-windows-actions");
        break;
    }
    default:
        break;
    }

    match.properties = properties;
    return match;
}

bool WindowsRunner::actionSupported(const Window *window, const WindowsRunnerAction action) const
{
    switch (action) {
    case CloseAction:
        return window->isCloseable();
    case MinimizeAction:
        return window->isMinimizable();
    case MaximizeAction:
        return window->isMaximizable();
    case ShadeAction:
        return window->isShadeable();
    case FullscreenAction:
        return window->isFullScreenable();
    case PinAction:
    case MoveAction:
        return !window->isSpecialWindow();
    case KeepAboveAction:
    case KeepBelowAction:
    case ActivateAction:
    default:
        return true;
    }
}

}
