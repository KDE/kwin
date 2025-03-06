/*
    SPDX-FileCopyrightText: 2024 Nicolas Fella <nicolas.fella@kdab.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "touchpadshortcuts.h"

#include "input.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>

namespace KWin
{

TouchpadShortcuts::TouchpadShortcuts()
{
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/modules/kded_touchpad"), this, QDBusConnection::ExportScriptableContents);
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.touchpad"));
}

void TouchpadShortcuts::toggle()
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

void TouchpadShortcuts::enable()
{
    enableOrDisableTouchpads(true);
}

void TouchpadShortcuts::disable()
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
