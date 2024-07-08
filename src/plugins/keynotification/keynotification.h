/*
    SPDX-FileCopyrightText: 2024 Nicolas Fella <nicolas.fella@kdab.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "effect/globals.h"
#include "plugin.h"

#include <KConfigGroup>
#include <KConfigWatcher>

namespace KWin
{
class KeyNotificationPlugin : public KWin::Plugin
{
    Q_OBJECT
public:
    explicit KeyNotificationPlugin();

private:
    void loadConfig(const KConfigGroup &group);
    void ledsChanged(KWin::LEDs leds);
    void modifiersChanged();
    void sendNotification(const QString &eventId, const QString &text);

    KConfigWatcher::Ptr m_configWatcher;
    bool m_enabled = false;
    KWin::LEDs m_currentLEDs;
    Qt::KeyboardModifiers m_currentModifiers;
};
}
