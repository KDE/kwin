/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "activities.h"
// KWin
#include "window.h"
#include "workspace.h"
// KDE
#include <KConfigGroup>
// Qt
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QFutureWatcher>
#include <QtConcurrentRun>

namespace KWin
{

Activities::Activities()
    : m_controller(new KActivities::Controller(this))
{
    connect(m_controller, &KActivities::Controller::activityRemoved, this, &Activities::slotRemoved);
    connect(m_controller, &KActivities::Controller::activityRemoved, this, &Activities::removed);
    connect(m_controller, &KActivities::Controller::activityAdded, this, &Activities::added);
    connect(m_controller, &KActivities::Controller::currentActivityChanged, this, &Activities::slotCurrentChanged);
    connect(m_controller, &KActivities::Controller::serviceStatusChanged, this, &Activities::slotServiceStatusChanged);
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
    const auto windows = Workspace::self()->allClientList();
    for (auto *const window : windows) {
        if (window->isDesktop()) {
            continue;
        }
        window->checkActivities();
    }
}

void Activities::setCurrent(const QString &activity)
{
    m_controller->setCurrentActivity(activity);
}

void Activities::slotCurrentChanged(const QString &newActivity)
{
    if (m_current == newActivity) {
        return;
    }
    m_previous = m_current;
    m_current = newActivity;
    Q_EMIT currentChanged(newActivity);
}

void Activities::slotRemoved(const QString &activity)
{
    const auto windows = Workspace::self()->allClientList();
    for (auto *const window : windows) {
        if (window->isDesktop()) {
            continue;
        }
        window->setOnActivity(activity, false);
    }
    // toss out any session data for it
    KConfigGroup cg(KSharedConfig::openConfig(), QByteArray("SubSession: ").append(activity.toUtf8()).constData());
    cg.deleteGroup();
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
    ws->updateClientArea();
}

bool Activities::start(const QString &id)
{
    Workspace *ws = Workspace::self();
    if (ws->sessionManager()->state() == SessionState::Saving) {
        return false; // ksmserver doesn't queue requests (yet)
    }

    if (!all().contains(id)) {
        return false; // bogus id
    }

    ws->sessionManager()->loadSubSessionInfo(id);

    QDBusInterface ksmserver("org.kde.ksmserver", "/KSMServer", "org.kde.KSMServerInterface");
    if (ksmserver.isValid()) {
        ksmserver.asyncCall("restoreSubSession", id);
    } else {
        qCDebug(KWIN_CORE) << "couldn't get ksmserver interface";
        return false;
    }
    return true;
}

bool Activities::stop(const QString &id)
{
    if (Workspace::self()->sessionManager()->state() == SessionState::Saving) {
        return false; // ksmserver doesn't queue requests (yet)
        // FIXME what about session *loading*?
    }

    // ugly hack to avoid dbus deadlocks
    QMetaObject::invokeMethod(
        this, [this, id] {
            reallyStop(id);
        },
        Qt::QueuedConnection);
    // then lie and assume it worked.
    return true;
}

void Activities::reallyStop(const QString &id)
{
    Workspace *ws = Workspace::self();
    if (ws->sessionManager()->state() == SessionState::Saving) {
        return; // ksmserver doesn't queue requests (yet)
    }

    qCDebug(KWIN_CORE) << id;

    QSet<QByteArray> saveSessionIds;
    QSet<QByteArray> dontCloseSessionIds;
    const auto windows = ws->allClientList();
    for (auto *const window : windows) {
        if (window->isDesktop()) {
            continue;
        }
        const QByteArray sessionId = window->sessionId();
        if (sessionId.isEmpty()) {
            continue; // TODO support old wm_command apps too?
        }

        // qDebug() << sessionId;

        // if it's on the activity that's closing, it needs saving
        // but if a process is on some other open activity, I don't wanna close it yet
        // this is, of course, complicated by a process having many windows.
        if (window->isOnAllActivities()) {
            dontCloseSessionIds << sessionId;
            continue;
        }

        const QStringList activities = window->activities();
        for (const QString &activityId : activities) {
            if (activityId == id) {
                saveSessionIds << sessionId;
            } else if (running().contains(activityId)) {
                dontCloseSessionIds << sessionId;
            }
        }
    }

    ws->sessionManager()->storeSubSession(id, saveSessionIds);

    QStringList saveAndClose;
    QStringList saveOnly;
    for (const QByteArray &sessionId : std::as_const(saveSessionIds)) {
        if (dontCloseSessionIds.contains(sessionId)) {
            saveOnly << sessionId;
        } else {
            saveAndClose << sessionId;
        }
    }

    qCDebug(KWIN_CORE) << "saveActivity" << id << saveAndClose << saveOnly;

    // pass off to ksmserver
    QDBusInterface ksmserver("org.kde.ksmserver", "/KSMServer", "org.kde.KSMServerInterface");
    if (ksmserver.isValid()) {
        ksmserver.asyncCall("saveSubSession", id, saveAndClose, saveOnly);
    } else {
        qCDebug(KWIN_CORE) << "couldn't get ksmserver interface";
    }
}

} // namespace
