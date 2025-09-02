/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "activities.h"
// KWin
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"
#if KWIN_BUILD_X11
#include "x11window.h"
#endif
// KDE
#include <KConfigGroup>
// Qt
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QFutureWatcher>
#include <QtConcurrentRun>

namespace KWin
{

Activities::Activities(const KSharedConfig::Ptr &config)
    : m_controller(new KActivities::Controller(this))
    , m_config(config)
{
    connect(m_controller, &KActivities::Controller::activityRemoved, this, &Activities::slotRemoved);
    connect(m_controller, &KActivities::Controller::activityRemoved, this, &Activities::removed);
    connect(m_controller, &KActivities::Controller::activityAdded, this, &Activities::added);
    connect(m_controller, &KActivities::Controller::currentActivityChanged, this, &Activities::slotCurrentChanged);
    connect(m_controller, &KActivities::Controller::serviceStatusChanged, this, &Activities::slotServiceStatusChanged);

    const auto group = m_config->group("Activities").group("LastVirtualDesktop");
    for (const auto &activity : group.keyList()) {
        const QString desktop = group.readEntry(activity);
        if (!desktop.isEmpty()) {
            m_lastVirtualDesktop[activity] = desktop;
        }
    }

    // Clean up no longer used subsession data
    const auto sessionsConfig = KSharedConfig::openConfig();
    const auto groups = sessionsConfig->groupList();
    for (const QString &groupName : groups) {
        if (groupName.startsWith(QLatin1String("SubSession: "))) {
            sessionsConfig->deleteGroup(groupName);
        }
    }
}

KActivities::Consumer::ServiceStatus Activities::serviceStatus() const
{
    return m_controller->serviceStatus();
}

void Activities::slotServiceStatusChanged()
{
    if (m_controller->serviceStatus() != KActivities::Consumer::Running) {
        return;
    }
    const auto windows = Workspace::self()->windows();
    for (auto *const window : windows) {
        if (!window->isClient()) {
            continue;
        }
        if (window->isDesktop()) {
            continue;
        }
        window->checkActivities();
    }
}

void Activities::setCurrent(const QString &activity, VirtualDesktop *desktop)
{
    if (desktop) {
        m_lastVirtualDesktop[activity] = desktop->id();
    }
    m_controller->setCurrentActivity(activity);
}

void Activities::notifyCurrentDesktopChanged(VirtualDesktop *desktop)
{
    m_lastVirtualDesktop[m_current] = desktop->id();
    m_config->group("Activities").group("LastVirtualDesktop").writeEntry(m_current, desktop->id());
}

void Activities::slotCurrentChanged(const QString &newActivity)
{
    if (m_current == newActivity) {
        return;
    }
    Q_EMIT currentAboutToChange();
    m_previous = m_current;
    m_current = newActivity;

    if (m_previous != nullUuid()) {
        m_lastVirtualDesktop[m_previous] = VirtualDesktopManager::self()->currentDesktop()->id();
    }
    const auto it = m_lastVirtualDesktop.find(m_current);
    if (it != m_lastVirtualDesktop.end()) {
        VirtualDesktop *desktop = VirtualDesktopManager::self()->desktopForId(it->second);
        if (desktop) {
            VirtualDesktopManager::self()->setCurrent(desktop);
        }
    }

    Q_EMIT currentChanged(newActivity);
}

void Activities::slotRemoved(const QString &activity)
{
    const auto windows = Workspace::self()->windows();
    for (auto *const window : windows) {
        if (!window->isClient()) {
            continue;
        }
        if (window->isDesktop()) {
            continue;
        }
        window->setOnActivity(activity, false);
    }

    m_lastVirtualDesktop.erase(activity);
    m_config->group("Activities").group("LastVirtualDesktop").deleteEntry(activity);
}

void Activities::toggleWindowOnActivity(Window *window, const QString &activity, bool dont_activate)
{
    // int old_desktop = window->desktop();
    bool was_on_activity = window->isOnActivity(activity);
    bool was_on_all = window->isOnAllActivities();
    // note: all activities === no activities
    bool enable = was_on_all || !was_on_activity;
    window->setOnActivity(activity, enable);
    if (window->isOnActivity(activity) == was_on_activity && window->isOnAllActivities() == was_on_all) { // No change
        return;
    }

    Workspace *ws = Workspace::self();
    if (window->isOnCurrentActivity()) {
        if (window->wantsTabFocus() && options->focusPolicyIsReasonable() && !was_on_activity && // for stickyness changes
                                                                                                 // FIXME not sure if the line above refers to the correct activity
            !dont_activate) {
            ws->requestFocus(window);
        } else {
            ws->restackWindowUnderActive(window);
        }
    } else {
        ws->raiseWindow(window);
    }

    // notifyWindowDesktopChanged( c, old_desktop );

    const auto transients_stacking_order = ws->ensureStackingOrder(window->transients());
    for (auto *const window : transients_stacking_order) {
        if (!window) {
            continue;
        }
        toggleWindowOnActivity(window, activity, dont_activate);
    }
    ws->rearrange();
}
} // namespace

#include "moc_activities.cpp"
