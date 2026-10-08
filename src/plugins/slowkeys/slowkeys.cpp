/*
    SPDX-FileCopyrightText: 2025 Martin Riethmayer <ripper@freakmail.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "slowkeys.h"
#include "effect/effecthandler.h"
#include "input_event.h"

namespace KWin
{

SlowKeysFilter::SlowKeysFilter()
    : InputEventFilter(InputFilterOrder::SlowKeys)
    , m_configWatcher(KConfigWatcher::create(KSharedConfig::openConfig("kaccessrc")))
{
    const QLatin1StringView groupName("Keyboard");
    connect(m_configWatcher.get(), &KConfigWatcher::configChanged, this, [this, groupName](const KConfigGroup &group) {
        if (group.name() == groupName) {
            loadConfig(group);
        }
    });
    loadConfig(m_configWatcher->config()->group(groupName));
}

void SlowKeysFilter::loadConfig(const KConfigGroup &group)
{
    input()->uninstallInputEventFilter(this);

    if (group.readEntry<bool>("SlowKeys", false)) {
        input()->installInputEventFilter(this);

        m_delay = std::chrono::milliseconds(group.readEntry<int>("SlowKeysDelay", 500));
        m_keysPressBeep = group.readEntry<bool>("SlowKeysPressBeep", false);
        m_keysAcceptBeep = group.readEntry<bool>("SlowKeysAcceptBeep", false);
        m_keysRejectBeep = group.readEntry<bool>("SlowKeysRejectBeep", false);
    } else {
        m_keys.clear();
    }
}

bool SlowKeysFilter::keyboardKey(KeyboardKeyEvent *event)
{
    const auto now = std::chrono::steady_clock::now();

    switch (event->state) {
    case KeyboardKeyState::Pressed:
    case KeyboardKeyState::Repeated:
        if (const auto it = m_keys.find(event->key); it == m_keys.end()) {
            // First time we're seeing this key, record the time when the key was pressed
            m_keys[event->key] = SlowKey{
                .pressTimestamp = now,
                .sentCount = 0,
            };

            if (m_keysPressBeep) {
                if (auto effect = effects->provides(Effect::SystemBell)) {
                    effect->perform(Effect::SystemBell, {});
                }
            }

            return true;
        } else {
            if (now - it->pressTimestamp < m_delay) {
                // The event occurred sooner than the user-set delay, we will reject the event
                if (m_keysRejectBeep) {
                    if (auto effect = effects->provides(Effect::SystemBell)) {
                        effect->perform(Effect::SystemBell, {});
                    }
                }
                return true;
            } else {
                // The event occurred later than the user-set delay, we will *not* reject the event
                if (m_keysAcceptBeep) {
                    if (auto effect = effects->provides(Effect::SystemBell)) {
                        effect->perform(Effect::SystemBell, {});
                    }
                }

                // Since we've rejected the event so far, we also need to update the "pressed" state.
                event->state = it->sentCount == 0 ? KeyboardKeyState::Pressed : KeyboardKeyState::Repeated;

                it->pressTimestamp = now;
                it->sentCount++;

                return false;
            }
        }
        Q_UNREACHABLE();

    case KeyboardKeyState::Released:
        if (const auto it = m_keys.find(event->key); it != m_keys.end()) {
            const bool seenAtLeastOnce = it->sentCount != 0;
            m_keys.erase(it);
            return !seenAtLeastOnce;
        }

        return false;
    }

    Q_UNREACHABLE_RETURN(false);
}

}
#include "moc_slowkeys.cpp"
