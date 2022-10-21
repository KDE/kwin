/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <kde@martin-graesslin.com>
    SPDX-FileCopyrightText: 2020 Benjamin Port <benjamin.port@enioka.com>
    SPDX-FileCopyrightText: 2022 Natalie Clarius <natalie_clarius@yahoo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowsrunnerinterface.h"

#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"

#include "krunner1adaptor.h"
#include <KLocalizedString>
#include <krunner/querymatch.h>
#include <qglobal.h>
#include <qnamespace.h>

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

    const QList<Window *> allWindows = workspace()->allClientList();
    const QVector<VirtualDesktop *> allDesktops = VirtualDesktopManager::self()->desktops() << nullptr; // add nullptr as dummy for new desktop

    QString terms = searchTerm.toLower();
    QString subjectTerms = searchTerm;
    QString objectTerms = searchTerm;
    QString kwd;

    const auto keyword = [](const char *term) {
        return i18nc("Note this is a KRunner keyword", term);
    };

    // match list keywords
    QString windowListKeyword;
    QString desktopListKeyword;
    if (terms.startsWith((kwd = keyword("window")))) {
        windowListKeyword = kwd;
        subjectTerms.remove(0, kwd.length());
    } else if (terms.startsWith((kwd = keyword("windows")))) {
        windowListKeyword = kwd;
        subjectTerms.remove(0, kwd.length());
    } else if (terms.startsWith((kwd = keyword("desktop")))) {
        desktopListKeyword = kwd;
        subjectTerms = subjectTerms.remove(0, kwd.length());
    } else if (terms.startsWith((kwd = keyword("desktops")))) {
        desktopListKeyword = kwd;
        subjectTerms.remove(0, kwd.length());
    }
    subjectTerms = subjectTerms.toLower().trimmed();

    // match action keywords
    QString actionKeyword;
    WindowsRunnerAction action = ActivateAction;
    if (terms.endsWith((kwd = keyword("activate")))) {
        actionKeyword = kwd;
        action = ActivateAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
    } else if (terms.endsWith((kwd = keyword("close")))) {
        actionKeyword = kwd;
        action = CloseAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
    } else if (terms.endsWith((kwd = keyword("min")))) {
        actionKeyword = kwd;
        action = MinimizeAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
    } else if (terms.endsWith((kwd = keyword("minimize")))) {
        actionKeyword = kwd;
        action = MinimizeAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
    } else if (terms.endsWith((kwd = keyword("unminimize")))) {
        actionKeyword = kwd;
        action = MinimizeAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
    } else if (terms.endsWith((kwd = keyword("max")))) {
        actionKeyword = kwd;
        action = MaximizeAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
    } else if (terms.endsWith((kwd = keyword("maximize")))) {
        actionKeyword = kwd;
        action = MaximizeAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
    } else if (terms.endsWith((kwd = keyword("unmaximize")))) {
        actionKeyword = kwd;
        action = MinimizeAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
    } else if (terms.endsWith((kwd = keyword("shade")))) {
        actionKeyword = kwd;
        action = ShadeAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
    } else if (terms.endsWith((kwd = keyword("unshade")))) {
        actionKeyword = kwd;
        action = MinimizeAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
    } else if (terms.endsWith((kwd = keyword("keep above")))) {
        actionKeyword = kwd;
        action = KeepAboveAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
    } else if (terms.endsWith((kwd = keyword("keep below")))) {
        actionKeyword = kwd;
        action = KeepBelowAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
    } else if (terms.endsWith((kwd = keyword("pin")))) {
        actionKeyword = kwd;
        action = PinAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
    } else if (terms.endsWith((kwd = keyword("unpin")))) {
        actionKeyword = kwd;
        action = MinimizeAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
    } else if (terms.contains((kwd = keyword("move"))) && !terms.endsWith(keyword("remove"))) {
        actionKeyword = kwd;
        action = MoveAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
        objectTerms.remove(0, objectTerms.lastIndexOf(kwd, -1, Qt::CaseInsensitive) + kwd.length());
    } else if (terms.endsWith((kwd = keyword("add")))) {
        actionKeyword = kwd;
        action = AddAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
    } else if (terms.endsWith((kwd = keyword("remove")))) {
        actionKeyword = kwd;
        action = RemoveAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
    } else if (terms.contains((kwd = keyword("rename")))) {
        actionKeyword = kwd;
        action = RenameAction;
        subjectTerms.truncate(subjectTerms.lastIndexOf(kwd));
        objectTerms.remove(0, objectTerms.lastIndexOf(kwd, -1, Qt::CaseInsensitive) + kwd.length());
    }
    subjectTerms = subjectTerms.trimmed();
    objectTerms = objectTerms.trimmed();

    // match window properties
    QString captionKeyword;
    QString appKeyword;
    QString desktopKeyword;
    QStringList propertyKeywords;
    for (const QString &term : subjectTerms.split(QLatin1Char(' '))) {
        if (term.isEmpty() || term.endsWith(QLatin1Char('='))) {
            continue;
        }
        if (!windowListKeyword.isEmpty() && term.startsWith(keyword("name") + QStringLiteral("="))) {
            captionKeyword = term.split(QStringLiteral("="))[1];
        } else if (!windowListKeyword.isEmpty() && term.startsWith(keyword("appname") + QStringLiteral("="))) {
            appKeyword = term.split(QStringLiteral("="))[1];
        } else if (!windowListKeyword.isEmpty() && term.startsWith(keyword("desktop") + QStringLiteral("="))) {
            desktopKeyword = term.split(QStringLiteral("="))[1];
        } else {
            propertyKeywords << term;
        }
    }

    // list windows
    for (const Window *window : allWindows) {
        if (!desktopListKeyword.isEmpty()) {
            break;
        }
        if (!actionSupported(window, action)) {
            continue;
        }
        if (!window->isNormalWindow() || window->resourceClass() == "krunner" || window->resourceClass() == "plasmashell" || !window->isOnCurrentDesktop()) {
            continue;
        }

        const QString caption = window->caption();
        const QString app = window->resourceClass();
        const QVector<VirtualDesktop *> desktops = window->desktops();

        // sort windows by stacking order (topmost first)
        // and their desktops by number (lowest first)
        qreal order1 = (workspace()->stackingOrder().size() - window->stackingOrder()) * 0.05;
        qreal order2 = window->isOnCurrentDesktop() ? 0 : 0.5;
        qreal order = order1 + order2;

        // each explicit keyword must match the given property
        if (!captionKeyword.isEmpty() && !caption.contains(captionKeyword, Qt::CaseInsensitive)) {
            continue;
        }
        if (!appKeyword.isEmpty() && app.contains(appKeyword), Qt::CaseInsensitive) {
            continue;
        }
        if (!desktopKeyword.isEmpty() && !std::any_of(desktops.begin(), desktops.end(), [desktopKeyword](const VirtualDesktop *desktop) {
                return desktop->name().contains(desktopKeyword, Qt::CaseInsensitive) || desktop->x11DesktopNumber() == desktopKeyword.toUInt();
            })
            && !window->isOnAllDesktops()) {
            continue;
        }

        // each free keyword must match at least one property
        bool propertiesMatch = true;
        bool fullWindowNameMatch = false;
        bool initialWindowNameMatch = false;
        bool partialWindowNameMatch = false;
        bool fullDesktopNameMatch = false;
        bool initialDesktopNameMatch = false;
        bool partialDesktopNameMatch = false;
        for (const QString &term : propertyKeywords) {
            if (caption.compare(term, Qt::CaseInsensitive) == 0 || app.compare(term, Qt::CaseInsensitive) == 0) {
                fullWindowNameMatch = true;
            } else if (caption.startsWith(term, Qt::CaseInsensitive) || app.startsWith(term, Qt::CaseInsensitive)) {
                initialWindowNameMatch = true;
            } else if (caption.contains(term, Qt::CaseInsensitive) || app.contains(term, Qt::CaseInsensitive)) {
                partialWindowNameMatch = true;
            } else if (std::any_of(desktops.begin(), desktops.end(), [term](const VirtualDesktop *desktop) {
                           return desktop->name().compare(term, Qt::CaseInsensitive) == 0;
                       })) {
                fullDesktopNameMatch = true;
            } else if (std::any_of(desktops.begin(), desktops.end(), [term](const VirtualDesktop *desktop) {
                           return desktop->name().startsWith(term, Qt::CaseInsensitive);
                       })) {
                initialDesktopNameMatch = true;
            } else if (std::any_of(desktops.begin(), desktops.end(), [term](const VirtualDesktop *desktop) {
                           return desktop->name().contains(term, Qt::CaseInsensitive);
                       })) {
                partialDesktopNameMatch = true;
            } else {
                propertiesMatch = false;
                break;
            }
        }
        if (!propertyKeywords.isEmpty() && !propertiesMatch) {
            continue;
        }

        if (action == MoveAction && !(partialWindowNameMatch || partialWindowNameMatch || window->isOnCurrentDesktop())) {
            continue; // don't compute move matches for all windows to avoid combinatorial explosion
        }

        qreal relevance = 0.0;
        Plasma::QueryMatch::Type type = Plasma::QueryMatch::NoMatch;
        if (!windowListKeyword.isEmpty()) {
            relevance = qMax(1.0, relevance);
            type = qMax(Plasma::QueryMatch::ExactMatch, type);
        }
        if (!actionKeyword.isEmpty()) {
            relevance = qMax(0.9, relevance);
            type = qMax(Plasma::QueryMatch::PossibleMatch, type);
        }
        if (!propertyKeywords.isEmpty()) {
            if (fullWindowNameMatch) {
                relevance = qMax(1.0, relevance);
                type = qMax(Plasma::QueryMatch::ExactMatch, type);
            }
            if (initialWindowNameMatch) {
                relevance = qMax(1.0, relevance);
                type = qMax(Plasma::QueryMatch::PossibleMatch, type);
            }
            if (partialWindowNameMatch) {
                relevance = qMax(0.7, relevance);
                type = qMax(Plasma::QueryMatch::PossibleMatch, type);
            }
            if (fullDesktopNameMatch) {
                relevance = qMax(0.6, relevance);
                type = qMax(Plasma::QueryMatch::PossibleMatch, type);
            }
            if (initialDesktopNameMatch) {
                relevance = qMax(0.6, relevance);
                type = qMax(Plasma::QueryMatch::PossibleMatch, type);
            }
            if (partialDesktopNameMatch) {
                relevance = qMax(0.5, relevance);
                type = qMax(Plasma::QueryMatch::PossibleMatch, type);
            }
        }
        if (relevance == 0) {
            continue;
        }

        switch (action) {
        case ActivateAction:
        case CloseAction:
        case MinimizeAction:
        case MaximizeAction:
        case FullscreenAction:
        case ShadeAction:
        case KeepAboveAction:
        case KeepBelowAction:
        case PinAction:
            matches << windowMatch(window, action, nullptr, relevance - order, type);
            break;
        case MoveAction:
            for (VirtualDesktop *d : allDesktops) {
                const QString dName = d ? d->name() : i18n("New Desktop");
                const uint dNumber = d ? d->x11DesktopNumber() : 0;
                qreal dOrder = d ? d->x11DesktopNumber() * 0.01 : 0.0;

                if (d && window->isOnDesktop(d)) {
                    continue;
                }
                if (!std::all_of(objectTerms.begin(), objectTerms.end(), [d, dName, dNumber](const QString &term) {
                        return dName.contains(term, Qt::CaseInsensitive) || (d && dNumber == term.toUInt());
                    })) {
                    continue;
                }

                matches << windowMatch(window, action, d, relevance - order - dOrder, type);
            }
            break;
        default:
            break;
        }
    }

    // list desktops
    for (VirtualDesktop *desktop : allDesktops) {
        if (!windowListKeyword.isEmpty()) {
            break;
        }
        if (!actionSupported(desktop, action)) {
            continue;
        }

        const QString name = desktop ? desktop->name() : i18n("New Desktop");
        const uint number = desktop ? desktop->x11DesktopNumber() : 0;
        const bool isCurrent = desktop ? desktop == VirtualDesktopManager::self()->currentDesktop() : false;

        // sort desktops by number (lowest first)
        qreal order = desktop ? (isCurrent ? 0 : desktop->x11DesktopNumber() * 0.01) : 0;

        bool propertiesMatch = true;
        bool exactNameMatch = false;
        bool possibleNameMatch = false;
        for (const QString &term : propertyKeywords) {
            if (name.startsWith(term, Qt::CaseInsensitive) || (desktop && number == term.toUInt())) {
                exactNameMatch = true;
            } else if (name.contains(term, Qt::CaseInsensitive)) {
                possibleNameMatch = true;
            } else {
                propertiesMatch = false;
                break;
            }
        }
        if (!propertyKeywords.isEmpty() && !propertiesMatch) {
            continue;
        }

        qreal relevance = 0.0;
        Plasma::QueryMatch::Type type = Plasma::QueryMatch::NoMatch;
        if (!desktopListKeyword.isEmpty()) {
            relevance = qMax(1.0, relevance);
            type = qMax(Plasma::QueryMatch::ExactMatch, type);
        }
        if (!actionKeyword.isEmpty() && windowListKeyword.isEmpty()) {
            relevance = qMax(0.9, relevance);
            type = qMax(Plasma::QueryMatch::PossibleMatch, type);
        }
        if (!propertyKeywords.isEmpty() && windowListKeyword.isEmpty()) {
            if (exactNameMatch) {
                relevance = qMax(0.8, relevance);
                type = qMax(Plasma::QueryMatch::ExactMatch, type);
            }
            if (possibleNameMatch) {
                relevance = qMax(0.7, relevance);
                type = qMax(Plasma::QueryMatch::PossibleMatch, type);
            }
        }
        if (relevance == 0) {
            continue;
        }

        switch (action) {
        case ActivateAction:
        case AddAction:
        case RemoveAction:
            matches << desktopMatch(desktop, action, nullptr, relevance - order, type);
            break;
        case RenameAction:
            matches << desktopMatch(desktop, action, &objectTerms, relevance - order, type);
            break;
        case CloseAction:
        case MinimizeAction:
            matches << windowsDesktopMatch(desktop, action, nullptr, relevance - order, type);
            break;
        case MoveAction:
            if (!(exactNameMatch || possibleNameMatch || isCurrent)) {
                break; // don't compute move matches for all desktops to avoid combinatorial explosion
            }
            for (VirtualDesktop *d : allDesktops) {
                const QString dName = d ? d->name() : i18n("New Desktop");
                const uint dNumber = d ? d->x11DesktopNumber() : 0;
                qreal dOrder = d ? d->x11DesktopNumber() * 0.01 : 0.0;

                if (d && d == desktop) {
                    continue;
                }
                if (!std::all_of(objectTerms.begin(), objectTerms.end(), [d, dName, dNumber](const QString term) {
                        return dName.contains(term, Qt::CaseInsensitive) || (d && dNumber == term.toUInt());
                    })) {
                    continue;
                }

                matches << windowsDesktopMatch(desktop, action, d, relevance - order - dOrder, type);
            }
            break;
        default:
            break;
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
    auto category = MatchCategory(parts[0].toInt());
    auto action = WindowsRunnerAction(parts[1].toInt());

    switch (category) {

    case WindowMatch: {
        const auto window = workspace()->findToplevel(QUuid::fromString(parts[2]));
        if (!window || !window->isClient()) {
            return;
        }
        switch (action) {
        case ActivateAction:
            if (window->isNormalWindow()) {
                workspace()->activateWindow(window);
            } else {
                workspace()->setShowingDesktop(true);
            }
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

    case DesktopMatch: {
        const auto desktop = vds->desktopForId(parts[2]);
        if (!desktop && action == RemoveAction) {
            return;
        }
        switch (action) {
        case ActivateAction: {
            const auto targetDesktop = desktop ? desktop : vds->createVirtualDesktop(vds->count());
            vds->setCurrent(targetDesktop);
            break;
        }
        case AddAction:
            vds->createVirtualDesktop(vds->count());
            break;
        case RemoveAction:
            vds->removeVirtualDesktop(desktop);
            break;
        case RenameAction: {
            const auto newName = parts[3];
            if (newName.isEmpty())
                return;
            desktop->setName(newName);
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
        case MinimizeAction:
            workspace()->toggleMinimizeAll();
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
        properties[QStringLiteral("subtext")] = i18n("Close window on %1", desktopName);
        break;
    case MinimizeAction:
        properties[QStringLiteral("subtext")] = i18n("(Un)minimize window on %1", desktopName);
        break;
    case MaximizeAction:
        properties[QStringLiteral("subtext")] = i18n("Maximize/restore window on %1", desktopName);
        break;
    case FullscreenAction:
        properties[QStringLiteral("subtext")] = i18n("Toggle fullscreen for window on %1", desktopName);
        break;
    case ShadeAction:
        properties[QStringLiteral("subtext")] = i18n("(Un)shade window on %1", desktopName);
        break;
    case KeepAboveAction:
        properties[QStringLiteral("subtext")] = i18n("Toggle keep above for window on %1", desktopName);
        break;
    case KeepBelowAction:
        properties[QStringLiteral("subtext")] = i18n("Toggle keep below window on %1", desktopName);
        break;
    case PinAction:
        properties[QStringLiteral("subtext")] = i18n("Toggle pin to all desktops window on %1", desktopName);
        break;
    case MoveAction: {
        const QString destinationDesktopName = destination ? destination->name() : i18n("New Desktop");
        match.text = window->caption() + QStringLiteral(" > ") + destinationDesktopName;
        properties[QStringLiteral("subtext")] = i18n("Move window on %1 to %2", desktopName, destinationDesktopName);
        break;
    }
    case ActivateAction:
    default:
        properties[QStringLiteral("subtext")] = i18n("Activate window on %1", desktopName);
        break;
    }
    match.properties = properties;
    return match;
}

RemoteMatch WindowsRunner::desktopMatch(const VirtualDesktop *desktop, const WindowsRunnerAction action, const QString *destination, qreal relevance, Plasma::QueryMatch::Type type) const
{

    RemoteMatch match;
    MatchCategory category = DesktopMatch;
    match.id = QString::number((int)category) + QLatin1Char('_') + QString::number((int)action) + QLatin1Char('_') + (desktop ? desktop->id() : "") + QLatin1Char('_') + (destination ? *destination : "");
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
    case AddAction:
        properties[QStringLiteral("subtext")] = i18n("Add a new desktop");
        break;
    case RemoveAction:
        properties[QStringLiteral("subtext")] = i18n("Remove desktop %1", desktop->name());
        break;
    case RenameAction:
        match.text = desktop->name() + QStringLiteral(" > ") + *destination;
        properties[QStringLiteral("subtext")] = i18n("Rename desktop %1 to %2", desktop->name(), *destination);
        break;
    default:
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
        properties[QStringLiteral("subtext")] = i18n("Close all windows on %1", desktop->name());
        break;
    case MinimizeAction:
        properties[QStringLiteral("subtext")] = i18n("Minimize all running windows on %1", desktop->name());
        break;
    case MoveAction: {
        const QString destinationDesktopName = destination ? destination->name() : i18n("New Desktop");
        match.text = desktop->name() + QStringLiteral(" > ") + destinationDesktopName;
        properties[QStringLiteral("subtext")] = i18n("Move all windows on %1 to %2", desktop->name(), destinationDesktopName);
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
    case ActivateAction:
        return true;
    case CloseAction:
        return window->isCloseable();
    case MinimizeAction:
        return window->isMinimizable();
    case MaximizeAction:
        return window->isMaximizable();
    case FullscreenAction:
        return window->isFullScreenable();
    case ShadeAction:
        return window->isShadeable();
    case KeepAboveAction:
    case KeepBelowAction:
        return window->isNormalWindow();
    case PinAction:
    case MoveAction:
        return !window->isSpecialWindow();
    default:
        return false;
    }
}

bool WindowsRunner::actionSupported(const VirtualDesktop *desktop, const WindowsRunnerAction action) const
{
    switch (action) {
    case ActivateAction:
        return true;
    case AddAction:
        return !desktop;
    case RemoveAction:
    case RenameAction:
    case CloseAction:
    case MinimizeAction:
    case MoveAction:
        return desktop;
    default:
        return false;
    }
}
}
