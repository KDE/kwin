/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "desktopbackgrounditem.h"
#include "abstract_client.h"
#include "abstract_output.h"
#ifdef KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "main.h"
#include "platform.h"
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
    setOutput(kwinApp()->platform()->findOutput(name));
}

AbstractOutput *DesktopBackgroundItem::output() const
{
    return m_output;
}

void DesktopBackgroundItem::setOutput(AbstractOutput *output)
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
#ifdef KWIN_BUILD_ACTIVITIES
        activity = Activities::self()->current();
#endif
    }

    const auto clients = workspace()->allClientList();
    for (AbstractClient *client : clients) {
        if (client->isDesktop() && client->isOnOutput(m_output) && client->isOnDesktop(desktop) && client->isOnActivity(activity)) {
            setClient(client);
            break;
        }
    }
}

} // namespace KWin
