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
#include <algorithm>
#include <endian.h>
#include <krunner/querymatch.h>
#include <qchar.h>
#include <qlist.h>
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
    } else if (term.endsWith(i18nc("Note this is a KRunner keyword", "move"), Qt::CaseInsensitive)) {
        action = MoveAction;
        term = term.left(term.lastIndexOf(i18nc("Note this is a KRunner keyword", "move")) - 1);
    } else if (term.endsWith(i18nc("Note this is a KRunner keyword", "add"), Qt::CaseInsensitive)) {
        action = AddDesktopAction;
        term = term.left(term.lastIndexOf(i18nc("Note this is a KRunner keyword", "add")) - 1);
    } else if (term.endsWith(i18nc("Note this is a KRunner keyword", "delete"), Qt::CaseInsensitive)) {
        action = RemoveDesktopAction;
        term = term.left(term.lastIndexOf(i18nc("Note this is a KRunner keyword", "delete")) - 1);
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

        qreal order = 0.0;
        for (auto it = Workspace::self()->stackingOrder().rbegin(); it != Workspace::self()->stackingOrder().rend(); ++it) {
            const Window *window = *it;
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
                switch (action) {
                case CloseAction:
                case MinimizeAction:
                case MaximizeAction:
                case ShadeAction:
                case FullscreenAction:
                case PinAction:
                case KeepAboveAction:
                case KeepBelowAction:
                case ActivateAction:
                    matches << windowMatch(window, action, nullptr, 1.0 - order);
                    break;
                case MoveAction: {
                    matches << windowMatch(window, MoveAction, nullptr, 1.0 - order);
                    qreal order_ = 0.0;
                    for (auto *desktop : vds->desktops()) {
                        if (!window->isOnDesktop(desktop)) {
                            matches << windowMatch(window, MoveAction, desktop, 1.0 - order - order_);
                            order += 0.01;
                        }
                    }
                    break;
                }
                default:
                    break;
                }
            }
            order += 0.05;
        }

        if (!matches.isEmpty()) {
            // the window keyword found matches - do not process other syntax possibilities
            return matches;
        }
    }

    // keyword match: when term starts with "windows" we list all desktops
    // the list can be restricted to desktops matching a given name
    for (auto *desktop : vds->desktops()) {
        if (term.startsWith("windows", Qt::CaseInsensitive) || term.contains(desktop->name(), Qt::CaseInsensitive)) {
            if (desktop != vds->currentDesktop()) {
                continue;
            }
            switch (action) {
            case CloseAction:
                matches << windowsDesktopMatch(desktop, action);
                break;
            case MoveAction: {
                matches << windowsDesktopMatch(desktop, action, nullptr);
                qreal order = 0.01;
                for (auto *d : vds->desktops()) {
                    if (d != desktop) {
                        matches << windowsDesktopMatch(desktop, action, d, 1.0 - order);
                        order += 0.01;
                    }
                }
                break;
            }
            default:
                break;
            }
        }
    }

    if (term.startsWith(i18nc("Note this is a KRunner keyword", "windows", Qt::CaseInsensitive)) && !matches.isEmpty()) {
        // the windows keyword found matches - do not process other syntax possibilities
        return matches;
    }

    bool desktopAdded = false;
    // check for desktop keyword
    if (term.startsWith(i18nc("Note this is a KRunner keyword", "desktop"), Qt::CaseInsensitive)) {
        switch (action) {
        case ActivateAction: {
            matches << desktopMatch(nullptr, action);
            qreal order = 0.01;
            for (auto desktop : vds->desktops()) {
                matches << desktopMatch(desktop, action, 1.0 - order);
                desktopAdded = true;
                order += 0.01;
            }
            break;
        }
        case AddDesktopAction: {
            matches << desktopMatch(nullptr, action);
            break;
        }
        case RemoveDesktopAction: {
            qreal order = 0.01;
            for (auto desktop : vds->desktops()) {
                if (desktop != vds->currentDesktop()) {
                    continue;
                }
                matches << desktopMatch(desktop, action, 1.0 - order);
                order += 0.01;
            }
            break;
        }
        default:
            break;
        }
        if (!matches.empty()) {
            return matches;
        }
    }

    // check for matching windows by name
    qreal order = 0.0;
    for (auto it = Workspace::self()->stackingOrder().rbegin(); it != Workspace::self()->stackingOrder().rend(); ++it) {
        const Window *window = *it;
        if (!(window->isNormalWindow() || (window->isDesktop() && window->isOnActiveOutput()))) {
            continue;
        }
        const QString appName = window->resourceClass();
        const QString name = window->caption();
        if (!window->isOnCurrentDesktop())
            continue;
        if (name.startsWith(term, Qt::CaseInsensitive) || appName.startsWith(term, Qt::CaseInsensitive)) {
            const qreal relevance = 0.8 - order;
            matches << windowMatch(window, action, nullptr, relevance, Plasma::QueryMatch::ExactMatch);
        } else if ((name.contains(term, Qt::CaseInsensitive) || appName.contains(term, Qt::CaseInsensitive)) && actionSupported(window, action)) {
            const qreal relevance = 0.7 - order;
            matches << windowMatch(window, action, nullptr, relevance, Plasma::QueryMatch::PossibleMatch);
        }
        order += 0.05;
    }

    // search all desktops and list desktops and the windows on them
    for (auto *desktop : vds->desktops()) {
        if (desktop->name().contains(term, Qt::CaseInsensitive)) {
            switch (action) {
            case ActivateAction:
                if (!desktopAdded && desktop != vds->currentDesktop()) {
                    matches << desktopMatch(desktop, action, 0.8);
                }
                break;
            case AddDesktopAction:
                break;
            case RemoveDesktopAction:
                matches << desktopMatch(desktop, action);
            default:
                break;
            }
            // search for windows on desktop and list them with less relevance
            qreal order = 0.0;
            for (auto it = Workspace::self()->stackingOrder().rbegin(); it != Workspace::self()->stackingOrder().rend(); ++it) {
                const Window *window = *it;
                if (!window->isNormalWindow()) {
                    continue;
                }
                if (window->isOnCurrentDesktop() && (window->desktops().contains(desktop) || window->isOnAllDesktops()) && actionSupported(window, action)) {
                    matches << windowMatch(window, action, nullptr, 0.5 - order, Plasma::QueryMatch::PossibleMatch);
                }
                order += 0.05;
            }
        }
    }
    if (term.startsWith(i18nc("Note this is a KRunner keyword", "new"))) {
        matches << desktopMatch(nullptr, action, 0.8);
    }

    return matches;
}

void WindowsRunner::Run(const QString &id, const QString &actionId)
{
    Q_UNUSED(actionId)
    VirtualDesktopManager *vds = VirtualDesktopManager::self();

    // Split id to get actionId and realId. We don't use actionId because our actions list is not constant
    const QStringList parts = id.split(QLatin1Char('_'));
    auto category = MatchCategory(parts[0].toInt());
    auto action = WindowsRunnerAction(parts[1].toInt());

    switch (category) {

    case DesktopMatch: {
        const auto desktop = vds->desktopForId(parts[2]);
        if (!desktop && action == RemoveDesktopAction) {
            return;
        }
        switch (action) {
        case ActivateAction: {
            const auto destinationDesktop = desktop ? desktop : vds->createVirtualDesktop(vds->count());
            vds->setCurrent(destinationDesktop);
            break;
        }
        case AddDesktopAction:
            vds->createVirtualDesktop(vds->count());
            break;
        case RemoveDesktopAction:
            vds->removeVirtualDesktop(desktop);
            break;
        default:
            break;
        }
        break;
    }

    case WindowMatch: {
        const auto window = workspace()->findToplevel(QUuid::fromString(parts[2]));
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
        case PinAction:
            window->setOnAllDesktops(!window->isOnAllDesktops());
            break;
        case MoveAction: {
            const auto destination = vds->desktopForId(parts[3]);
            const auto destinationDesktop = destination ? destination : vds->createVirtualDesktop(vds->count());
            window->setDesktops({destinationDesktop});
            break;
        }
        default:
            break;
        }
        break;
    }

    case WindowsDesktopMatch: {
        const auto desktop = vds->desktopForId(parts[2]);
        if (!desktop) {
            return;
        }
        QList<KWin::Window *> windows;
        std::copy_if(workspace()->allClientList().begin(), workspace()->allClientList().end(), std::back_inserter(windows), [desktop, action, this](Window *window) {
            return window->isNormalWindow() && window->isOnDesktop(desktop) && actionSupported(window, action);
        });
        switch (action) {
        case CloseAction:
            for (Window *window : windows) {
                window->closeWindow();
            }
            break;
        case MoveAction: {
            const auto destination = vds->desktopForId(parts[3]);
            const auto destinationDesktop = destination ? destination : vds->createVirtualDesktop(vds->count());
            for (Window *window : windows) {
                window->setDesktops({destinationDesktop});
            }
            break;
        }
        default:
            break;
        }
    } break;
    }
}

RemoteMatch WindowsRunner::desktopMatch(const VirtualDesktop *desktop, const WindowsRunnerAction action, qreal relevance, Plasma::QueryMatch::Type type) const
{
    RemoteMatch match;
    MatchCategory category = DesktopMatch;
    match.id = QString::number((int)category) + QLatin1Char('_') + QString::number((int)action) + QLatin1Char('_') + (desktop ? desktop->id() : "") + QLatin1Char('_');
    match.type = type;
    match.iconName = QStringLiteral("virtual-desktops");
    match.relevance = relevance;
    match.type = type;
    match.text = desktop ? desktop->name() : i18n("New Desktop");

    QVariantMap properties;

    switch (action) {
    case ActivateAction:
        properties[QStringLiteral("subtext")] = desktop ? i18n("Switch to desktop %1", desktop->name()) : i18n("Add a new desktop and switch to it");
        break;
    case AddDesktopAction:
        properties[QStringLiteral("subtext")] = i18n("Add a new desktop");
        break;
    case RemoveDesktopAction:
        properties[QStringLiteral("subtext")] = i18n("Remove desktop %1", desktop->name());
        break;
    default:
        break;
    }
    match.properties = properties;
    return match;
}

RemoteMatch WindowsRunner::windowMatch(const Window *window, const WindowsRunnerAction action, const VirtualDesktop *destination, qreal relevance, Plasma::QueryMatch::Type type) const
{
    RemoteMatch match;
    MatchCategory category = WindowMatch;
    match.id = QString::number((int)category) + QLatin1Char('_') + QString::number((int)action) + QLatin1Char('_') + window->internalId().toString() + QLatin1Char('_') + (destination ? destination->id() : "");
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
    case MoveAction: {
        const QString destinationDesktopName = destination ? destination->name() : i18n("New Desktop");
        match.text = window->caption() + QStringLiteral(" > ") + destinationDesktopName;
        properties[QStringLiteral("subtext")] = i18n("Move running window on %1 to %2", desktopName, destinationDesktopName);
        break;
    }
    case ActivateAction:
    default:
        properties[QStringLiteral("subtext")] = i18n("Activate running window on %1", desktopName);
        break;
    }
    match.properties = properties;
    return match;
}

RemoteMatch WindowsRunner::windowsDesktopMatch(const VirtualDesktop *desktop, const WindowsRunnerAction action, const VirtualDesktop *destination, qreal relevance, Plasma::QueryMatch::Type type) const
{
    RemoteMatch match;
    MatchCategory category = WindowsDesktopMatch;
    match.id = QString::number((int)category) + QLatin1Char('_') + QString::number((int)action) + QLatin1Char('_') + desktop->id() + QLatin1Char('_') + (destination ? destination->id() : "");
    match.type = type;
    match.iconName = QStringLiteral("virtual-desktops");
    match.relevance = relevance;
    match.type = type;
    match.text = desktop->name();

    QVariantMap properties;

    switch (action) {
    case CloseAction:
        properties[QStringLiteral("subtext")] = i18n("Close all running windows on %1", desktop->name());
        break;
    case MoveAction: {
        const QString destinationDesktopName = destination ? destination->name() : i18n("New Desktop");
        match.text = desktop->name() + QStringLiteral(" > ") + destinationDesktopName;
        properties[QStringLiteral("subtext")] = i18n("Move all running windows on %1 to %2", desktop->name(), destinationDesktopName);
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
