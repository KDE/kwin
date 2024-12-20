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

RemoteMatches WindowsRunner::Match(const QString &searchTerm)
{
    RemoteMatches matches;

    QString term = searchTerm;
    WindowsRunnerAction action = ActivateAction;
    if (QString keyword = i18nc("Note this is a KRunner keyword", "activate"); term.endsWith(keyword, Qt::CaseInsensitive)) {
        action = ActivateAction;
        term = term.left(term.lastIndexOf(keyword) - 1);
    } else if (QString keyword = i18nc("Note this is a KRunner keyword", "close"); term.endsWith(keyword, Qt::CaseInsensitive)) {
        action = CloseAction;
        term = term.left(term.lastIndexOf(keyword) - 1);
    } else if (QString keyword = i18nc("Note this is a KRunner keyword", "min"); term.endsWith(keyword, Qt::CaseInsensitive)) {
        action = MinimizeAction;
        term = term.left(term.lastIndexOf(keyword) - 1);
    } else if (QString keyword = i18nc("Note this is a KRunner keyword", "minimize"); term.endsWith(keyword, Qt::CaseInsensitive)) {
        action = MinimizeAction;
        term = term.left(term.lastIndexOf(keyword) - 1);
    } else if (QString keyword = i18nc("Note this is a KRunner keyword", "max"); term.endsWith(keyword, Qt::CaseInsensitive)) {
        action = MaximizeAction;
        term = term.left(term.lastIndexOf(keyword) - 1);
    } else if (term.endsWith(i18nc("Note this is a KRunner keyword", "maximize"), Qt::CaseInsensitive)) {
        action = MaximizeAction;
        term = term.left(term.lastIndexOf(i18nc("Note this is a KRunner keyword", "maximize")) - 1);
    } else if (QString keyword = i18nc("Note this is a KRunner keyword", "fullscreen"); term.endsWith(keyword, Qt::CaseInsensitive)) {
        action = FullscreenAction;
        term = term.left(term.lastIndexOf(keyword) - 1);
    } else if (QString keyword = i18nc("Note this is a KRunner keyword", "shade"); term.endsWith(keyword, Qt::CaseInsensitive)) {
        action = ShadeAction;
        term = term.left(term.lastIndexOf(keyword) - 1);
    } else if (QString keyword = i18nc("Note this is a KRunner keyword", "keep above"); term.endsWith(keyword, Qt::CaseInsensitive)) {
        action = KeepAboveAction;
        term = term.left(term.lastIndexOf(keyword) - 1);
    } else if (QString keyword = i18nc("Note this is a KRunner keyword", "keep below"); term.endsWith(keyword, Qt::CaseInsensitive)) {
        action = KeepBelowAction;
        term = term.left(term.lastIndexOf(keyword) - 1);
    }

    // keyword match: when term starts with "window" we list all windows
    // the list can be restricted to windows matching a given name, class, role or desktop
    if (term.startsWith(i18nc("Note this is a KRunner keyword", "window"), Qt::CaseInsensitive)) {
        const QStringList keywords = term.split(QLatin1Char(' '));
        QString windowName;
        QString windowAppName;
        VirtualDesktop *targetDesktop = nullptr;
        QVariant desktopId;
        for (const QString &keyword : keywords) {
            if (keyword.endsWith(QLatin1Char('='))) {
                continue;
            }
            if (keyword.startsWith(i18nc("Note this is a KRunner keyword", "name") + QLatin1Char('='), Qt::CaseInsensitive)) {
                windowName = keyword.split(QLatin1Char('='))[1];
            } else if (keyword.startsWith(i18nc("Note this is a KRunner keyword", "appname") + QLatin1Char('='), Qt::CaseInsensitive)) {
                windowAppName = keyword.split(QLatin1Char('='))[1];
            } else if (keyword.startsWith(i18nc("Note this is a KRunner keyword", "desktop") + QLatin1Char('='), Qt::CaseInsensitive)) {
                desktopId = keyword.split(QLatin1Char('='))[1];
                for (const auto desktop : VirtualDesktopManager::self()->desktops()) {
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

        for (const Window *window : Workspace::self()->windows()) {
            if (window->isUnmanaged()) {
                continue;
            }
            if (!window->isNormalWindow()) {
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
                matches << windowsMatch(window, action);
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
            for (auto desktop : VirtualDesktopManager::self()->desktops()) {
                matches << desktopMatch(desktop);
                desktopAdded = true;
            }
        }
    }

    // check for matching desktops by name
    for (const Window *window : Workspace::self()->windows()) {
        if (window->isUnmanaged()) {
            continue;
        }
        if (!window->isNormalWindow()) {
            continue;
        }
        const QString appName = window->resourceClass();
        const QString name = window->caption();
        if (name.startsWith(term, Qt::CaseInsensitive) || appName.startsWith(term, Qt::CaseInsensitive)) {
            matches << windowsMatch(window, action, 0.8, HighestCategoryRelevance);
        } else if ((name.contains(term, Qt::CaseInsensitive) || appName.contains(term, Qt::CaseInsensitive)) && actionSupported(window, action)) {
            matches << windowsMatch(window, action, 0.7, LowCategoryRelevance);
        }
    }

    for (auto *desktop : VirtualDesktopManager::self()->desktops()) {
        if (desktop->name().contains(term, Qt::CaseInsensitive)) {
            if (!desktopAdded && desktop != VirtualDesktopManager::self()->currentDesktop()) {
                matches << desktopMatch(desktop, ActivateDesktopAction, 0.8);
            }
            // search for windows on desktop and list them with less relevance
            for (const Window *window : Workspace::self()->windows()) {
                if (window->isUnmanaged()) {
                    continue;
                }
                if (!window->isNormalWindow()) {
                    continue;
                }
                if ((window->desktops().contains(desktop) || window->isOnAllDesktops()) && actionSupported(window, action)) {
                    matches << windowsMatch(window, action, 0.5, LowCategoryRelevance);
                }
            }
        }
    }

    return matches;
}

void WindowsRunner::Run(const QString &id, const QString &actionId)
{
    // Split id to get actionId and realId. We don't use actionId because our actions list is not constant
    const QStringList parts = id.split(QLatin1Char('_'));
    if (parts.size() != 2) {
        return;
    }

    const auto action = WindowsRunnerAction(parts[0].toInt());
    const auto objectId = parts[1];

    if (action == ActivateDesktopAction) {
        QByteArray desktopId = objectId.toLocal8Bit();
        auto desktop = VirtualDesktopManager::self()->desktopForId(desktopId);
        if (!desktop) {
            return;
        }
        VirtualDesktopManager::self()->setCurrent(desktop);
        return;
    }

    const auto window = workspace()->findWindow(QUuid::fromString(objectId));
    if (!window || !window->isClient()) {
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
    case ActivateDesktopAction:
        Q_UNREACHABLE();
    }
}

RemoteMatch WindowsRunner::desktopMatch(const VirtualDesktop *desktop, const WindowsRunnerAction action, qreal relevance) const
{
    RemoteMatch match;
    match.id = QString::number(action) + QLatin1Char('_') + desktop->id();
    match.categoryRelevance = HighestCategoryRelevance;
    match.iconName = QStringLiteral("user-desktop");
    match.text = desktop->name();
    match.relevance = relevance;
    match.properties.insert(QStringLiteral("subtext"), i18n("Switch to desktop %1", desktop->name()));
    return match;
}

RemoteMatch WindowsRunner::windowsMatch(const Window *window, const WindowsRunnerAction action, qreal relevance, qreal categoryRelevance) const
{
    RemoteMatch match;
    match.id = QString::number((int)action) + QLatin1Char('_') + window->internalId().toString();
    match.text = window->caption();
    match.iconName = window->icon().name();
    match.relevance = relevance;
    match.categoryRelevance = categoryRelevance;

    const QList<VirtualDesktop *> desktops = window->desktops();
    bool allDesktops = window->isOnAllDesktops();

    const VirtualDesktop *targetDesktop = VirtualDesktopManager::self()->currentDesktop();
    // Show on current desktop unless window is only attached to other desktop, in this case show on the first attached desktop
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
        match.properties.insert(QStringLiteral("icon-data"), QVariant::fromValue(remoteImage));
    }

    const QString desktopName = targetDesktop->name();
    switch (action) {
    case CloseAction:
        match.properties[QStringLiteral("subtext")] = i18n("Close running window on %1", desktopName);
        break;
    case MinimizeAction:
        match.properties[QStringLiteral("subtext")] = i18n("(Un)minimize running window on %1", desktopName);
        break;
    case MaximizeAction:
        match.properties[QStringLiteral("subtext")] = i18n("Maximize/restore running window on %1", desktopName);
        break;
    case FullscreenAction:
        match.properties[QStringLiteral("subtext")] = i18n("Toggle fullscreen for running window on %1", desktopName);
        break;
    case ShadeAction:
        match.properties[QStringLiteral("subtext")] = i18n("(Un)shade running window on %1", desktopName);
        break;
    case KeepAboveAction:
        match.properties[QStringLiteral("subtext")] = i18n("Toggle keep above for running window on %1", desktopName);
        break;
    case KeepBelowAction:
        match.properties[QStringLiteral("subtext")] = i18n("Toggle keep below running window on %1", desktopName);
        break;
    case ActivateAction:
    default:
        match.properties[QStringLiteral("subtext")] = i18n("Activate running window on %1", desktopName);
        break;
    }
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
    case KeepAboveAction:
    case KeepBelowAction:
    case ActivateAction:
    default:
        return true;
    }
}

}

#include "moc_windowsrunnerinterface.cpp"
