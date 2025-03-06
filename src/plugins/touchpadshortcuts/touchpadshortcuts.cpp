/*
    SPDX-FileCopyrightText: 2024 Nicolas Fella <nicolas.fella@kdab.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "touchpadshortcuts.h"

#include "input.h"

#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>

#include <KGlobalAccel>
#include <KLocalizedString>

namespace KWin
{

static const QString s_touchpadComponent = QStringLiteral("kcm_touchpad");

TouchpadShortcuts::TouchpadShortcuts()
{
    QAction *touchpadToggleAction = new QAction(this);
    QAction *touchpadOnAction = new QAction(this);
    QAction *touchpadOffAction = new QAction(this);

    const QString touchpadDisplayName = i18n("Touchpad");

    touchpadToggleAction->setObjectName(QStringLiteral("Toggle Touchpad"));
    touchpadToggleAction->setProperty("componentName", s_touchpadComponent);
    touchpadToggleAction->setProperty("componentDisplayName", touchpadDisplayName);
    touchpadOnAction->setObjectName(QStringLiteral("Enable Touchpad"));
    touchpadOnAction->setProperty("componentName", s_touchpadComponent);
    touchpadOnAction->setProperty("componentDisplayName", touchpadDisplayName);
    touchpadOffAction->setObjectName(QStringLiteral("Disable Touchpad"));
    touchpadOffAction->setProperty("componentName", s_touchpadComponent);
    touchpadOffAction->setProperty("componentDisplayName", touchpadDisplayName);
    KGlobalAccel::self()->setGlobalShortcut(touchpadToggleAction, QList<QKeySequence>{Qt::Key_TouchpadToggle, Qt::ControlModifier | Qt::MetaModifier | Qt::Key_TouchpadToggle, Qt::ControlModifier | Qt::MetaModifier | Qt::Key_Zenkaku_Hankaku});
    KGlobalAccel::self()->setGlobalShortcut(touchpadOnAction, QList<QKeySequence>{Qt::Key_TouchpadOn});
    KGlobalAccel::self()->setGlobalShortcut(touchpadOffAction, QList<QKeySequence>{Qt::Key_TouchpadOff});
    connect(touchpadToggleAction, &QAction::triggered, this, &TouchpadShortcuts::toggleTouchpads);
    connect(touchpadOnAction, &QAction::triggered, this, &TouchpadShortcuts::enableTouchpads);
    connect(touchpadOffAction, &QAction::triggered, this, &TouchpadShortcuts::disableTouchpads);
}

void TouchpadShortcuts::toggleTouchpads()
{
    bool enabled = true;
    const auto devices = input()->devices();
    for (InputDevice *device : devices) {
        if (!device->isTouchpad()) {
            continue;
        }

        if (!device->isEnabled()) {
            enabled = false;
            break;
        }
    }

    enableOrDisableTouchpads(!enabled);
}

void TouchpadShortcuts::enableTouchpads()
{
    enableOrDisableTouchpads(true);
}

void TouchpadShortcuts::disableTouchpads()
{
    enableOrDisableTouchpads(false);
}

void TouchpadShortcuts::enableOrDisableTouchpads(bool enable)
{

    bool changed = false;
    const auto devices = input()->devices();
    for (InputDevice *device : devices) {
        if (!device->isTouchpad()) {
            continue;
        }
        if (device->isEnabled() != enable) {
            device->setEnabled(enable);
            changed = true;
        }
    }
    // send OSD message
    if (changed) {
        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
                                                          QStringLiteral("/org/kde/osdService"),
                                                          QStringLiteral("org.kde.osdService"),
                                                          QStringLiteral("touchpadEnabledChanged"));
        msg.setArguments({enable});
        QDBusConnection::sessionBus().asyncCall(msg);
    }
}

}

#include "moc_touchpadshortcuts.cpp"
