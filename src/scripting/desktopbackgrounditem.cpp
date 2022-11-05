/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "desktopbackgrounditem.h"
#include "core/output.h"
#include "window.h"
#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "core/outputbackend.h"
#include "main.h"
#include "scripting_logging.h"
#include "virtualdesktops.h"
#include "workspace.h"

namespace KWin
{

DesktopBackgroundItem::DesktopBackgroundItem(QQuickItem *parent)
    : WindowThumbnailItem(parent)
{
}

void DesktopBackgroundItem::componentComplete()
{
    WindowThumbnailItem::componentComplete();
    updateWindow();
}

QString DesktopBackgroundItem::outputName() const
{
    return m_output ? m_output->name() : QString();
}

void DesktopBackgroundItem::setOutputName(const QString &name)
{
    setOutput(kwinApp()->outputBackend()->findOutput(name));
}

Output *DesktopBackgroundItem::output() const
{
    return m_output;
}

void DesktopBackgroundItem::setOutput(Output *output)
{
    if (m_output != output) {
        m_output = output;
        updateWindow();
        Q_EMIT outputChanged();
    }
}

VirtualDesktop *DesktopBackgroundItem::desktop() const
{
    return m_desktop;
}

void DesktopBackgroundItem::setDesktop(VirtualDesktop *desktop)
{
    if (m_desktop != desktop) {
        m_desktop = desktop;
        updateWindow();
        Q_EMIT desktopChanged();
    }
}

QString DesktopBackgroundItem::activity() const
{
    return m_activity;
}

void DesktopBackgroundItem::setActivity(const QString &activity)
{
    if (m_activity != activity) {
        m_activity = activity;
        updateWindow();
        Q_EMIT activityChanged();
    }
}

void DesktopBackgroundItem::updateWindow()
{
    if (!isComponentComplete()) {
        return;
    }

    if (Q_UNLIKELY(!m_output)) {
        qCWarning(KWIN_SCRIPTING) << "DesktopBackgroundItem.output is required";
        return;
    }

    VirtualDesktop *desktop = m_desktop;
    if (!desktop) {
        desktop = VirtualDesktopManager::self()->currentDesktop();
    }

    QString activity = m_activity;
    if (activity.isEmpty()) {
#if KWIN_BUILD_ACTIVITIES
        activity = Workspace::self()->activities()->current();
#endif
    }

    Window *clientCandidate = nullptr;

    const auto clients = workspace()->allClientList();
    for (Window *client : clients) {
        if (client->isDesktop() && client->isOnOutput(m_output) && client->isOnDesktop(desktop) && client->isOnActivity(activity)) {
            // In the unlikely event there are multiple desktop windows (e.g. conky's floating panel is of type "desktop")
            // choose the one which matches the ouptut size, if possible.
            if (!clientCandidate || client->size() == m_output->geometry().size()) {
                clientCandidate = client;
            }
        }
    }

    setClient(clientCandidate);
}

} // namespace KWin

#include "moc_desktopbackgrounditem.cpp"
