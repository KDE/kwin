/*
    SPDX-FileCopyrightText: 2025 Martin Riethmayer <ripper@freakmail.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "slowkeys.h"
#include "core/inputdevice.h"
#include "effect/effecthandler.h"
#include "input_event.h"
#include "keyboard_input.h"
#include <chrono>
#include <wayland-client-protocol.h>

namespace KWin
{

SlowKeysFilter::SlowKeysFilter()
    : InputEventFilter(InputFilterOrder::SlowKeys)
    , m_configWatcher(KConfigWatcher::create(KSharedConfig::openConfig("kaccessrc")))
{
    const QLatin1String groupName("Keyboard");
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
        m_firstEvent.clear();
    }
}

bool SlowKeysFilter::keyboardKey(KeyboardKeyEvent *event)
{
    std::chrono::milliseconds now = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

    switch (event->state) {
    case KeyboardKeyState::Pressed:
    case KeyboardKeyState::Repeated:
        if (auto it = m_firstEvent.find(event->key); it == m_firstEvent.end()) {
            // First time we're seing this key, record the time when the key was pressed
            m_firstEvent[event->key] = now;
            if (m_keysPressBeep) {
                if (auto effect = effects->provides(Effect::SystemBell)) {
                    effect->perform(Effect::SystemBell, {});
                }
            }

            return true;
        } else {
            auto first = *it;

            if (now - first < m_delay) {
                // The event occured sooner than the user-set delay, we will reject the event
                if (m_keysRejectBeep) {
                    if (auto effect = effects->provides(Effect::SystemBell)) {
                        effect->perform(Effect::SystemBell, {});
                    }
                }
                return true;
            } else {
                // The event occured later than the user-set delay, we will *not* reject the event
                if (m_keysAcceptBeep) {
                    if (auto effect = effects->provides(Effect::SystemBell)) {
                        effect->perform(Effect::SystemBell, {});
                    }
                }
                m_firstEvent.remove(event->key);
                // Since we've rejected the event so far, we also need to update the "pressed" state.
                event->state = KeyboardKeyState::Pressed;
                return false;
            }
        }
        Q_UNREACHABLE();
    case KeyboardKeyState::Released:
        // This will make sure that we do not send a release event for a key press that was filtered out (i.e. not marked as pressed)
        return m_firstEvent.remove(event->key);
    }

    Q_UNREACHABLE();
}
}
#include "moc_slowkeys.cpp"
