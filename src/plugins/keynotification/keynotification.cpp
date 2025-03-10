/*
    SPDX-FileCopyrightText: 2024 Nicolas Fella <nicolas.fella@kdab.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "keynotification.h"
#include "effect/effecthandler.h"
#include "keyboard_input.h"
#include "xkb.h"

#include <KLocalizedString>
#include <KNotification>

namespace KWin
{
KeyNotificationPlugin::KeyNotificationPlugin()
    : m_configWatcher(KConfigWatcher::create(KSharedConfig::openConfig("kaccessrc")))
{
    const QLatin1String groupName("Keyboard");
    connect(m_configWatcher.get(), &KConfigWatcher::configChanged, this, [this, groupName](const KConfigGroup &group) {
        if (group.name() == groupName) {
            loadConfig(group);
        }
    });
    loadConfig(m_configWatcher->config()->group(groupName));

    connect(input()->keyboard(), &KeyboardInputRedirection::ledsChanged, this,
            &KeyNotificationPlugin::ledsChanged);

    connect(input()->keyboard()->xkb(), &Xkb::modifierStateChanged, this, &KeyNotificationPlugin::modifiersChanged);
}

void KeyNotificationPlugin::ledsChanged(LEDs leds)
{
    if (m_useBellWhenLocksChange) {
        if (auto effect = effects->provides(Effect::SystemBell)) {
            effect->perform(Effect::SystemBell, {});
        }
    }

    if (m_enabled) {
        if (!m_currentLEDs.testFlag(LED::CapsLock) && leds.testFlag(LED::CapsLock)) {
            sendNotification("lockkey-locked", i18n("The Caps Lock key has been activated"));
        }

        if (m_currentLEDs.testFlag(LED::CapsLock) && !leds.testFlag(LED::CapsLock)) {
            sendNotification("lockkey-unlocked", i18n("The Caps Lock key is now inactive"));
        }

        if (!m_currentLEDs.testFlag(LED::NumLock) && leds.testFlag(LED::NumLock)) {
            sendNotification("lockkey-locked", i18n("The Num Lock key has been activated"));
        }

        if (m_currentLEDs.testFlag(LED::NumLock) && !leds.testFlag(LED::NumLock)) {
            sendNotification("lockkey-unlocked", i18n("The Num Lock key is now inactive"));
        }

        if (!m_currentLEDs.testFlag(LED::ScrollLock) && leds.testFlag(LED::ScrollLock)) {
            sendNotification("lockkey-locked", i18n("The Scroll Lock key has been activated"));
        }

        if (m_currentLEDs.testFlag(LED::ScrollLock) && !leds.testFlag(LED::ScrollLock)) {
            sendNotification("lockkey-unlocked", i18n("The Scroll Lock key is now inactive"));
        }
    }

    m_currentLEDs = leds;
}

void KeyNotificationPlugin::modifiersChanged()
{
    Qt::KeyboardModifiers mods = input()->keyboard()->xkb()->modifiers();

    if (m_enabled) {
        if (!m_currentModifiers.testFlag(Qt::ShiftModifier) && mods.testFlag(Qt::ShiftModifier)) {
            sendNotification("modifierkey-latched", i18n("The Shift key is now active."));
        }

        if (m_currentModifiers.testFlag(Qt::ShiftModifier) && !mods.testFlag(Qt::ShiftModifier)) {
            sendNotification("modifierkey-unlatched", i18n("The Shift key is now inactive."));
        }

        if (!m_currentModifiers.testFlag(Qt::ControlModifier) && mods.testFlag(Qt::ControlModifier)) {
            sendNotification("modifierkey-latched", i18n("The Control key is now active."));
        }

        if (m_currentModifiers.testFlag(Qt::ControlModifier) && !mods.testFlag(Qt::ControlModifier)) {
            sendNotification("modifierkey-unlatched", i18n("The Control key is now inactive."));
        }

        if (!m_currentModifiers.testFlag(Qt::AltModifier) && mods.testFlag(Qt::AltModifier)) {
            sendNotification("modifierkey-latched", i18n("The Alt key is now active."));
        }

        if (m_currentModifiers.testFlag(Qt::AltModifier) && !mods.testFlag(Qt::AltModifier)) {
            sendNotification("modifierkey-unlatched", i18n("The Alt key is now inactive."));
        }

        if (!m_currentModifiers.testFlag(Qt::MetaModifier) && mods.testFlag(Qt::MetaModifier)) {
            sendNotification("modifierkey-latched", i18n("The Meta key is now active."));
        }

        if (m_currentModifiers.testFlag(Qt::MetaModifier) && !mods.testFlag(Qt::MetaModifier)) {
            sendNotification("modifierkey-unlatched", i18n("The Meta key is now inactive."));
        }
    }

    m_currentModifiers = input()->keyboard()->xkb()->modifiers();
}

void KeyNotificationPlugin::sendNotification(const QString &eventId, const QString &text)
{
    KNotification *notification = new KNotification(eventId);
    notification->setComponentName(QStringLiteral("kaccess"));
    notification->setText(text);
    notification->sendEvent();
}

void KeyNotificationPlugin::loadConfig(const KConfigGroup &group)
{
    m_enabled = group.readEntry("kNotifyModifiers", false);
    m_useBellWhenLocksChange = group.readEntry("ToggleKeysBeep", false);
}
}

#include "moc_keynotification.cpp"
