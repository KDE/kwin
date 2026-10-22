/*
    SPDX-FileCopyrightText: 2022 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "stickykeys.h"
#include "effect/effecthandler.h"
#include "keyboard_input.h"
#include "xkb.h"

#include <QTimer>

#include <KLazyLocalizedString>
#if KWIN_BUILD_NOTIFICATIONS
#include <KNotification>
#endif

namespace KWin
{

struct Modifier
{
    Qt::Key key;
    KLazyLocalizedString lockedText;
};

static const std::array<Modifier, 5> modifiers = {
    Modifier{Qt::Key_Shift, kli18n("The Shift key has been locked and is now active for all of the following keypresses.")},
    Modifier{Qt::Key_Control, kli18n("The Control key has been locked and is now active for all of the following keypresses.")},
    Modifier{Qt::Key_Alt, kli18n("The Alt key has been locked and is now active for all of the following keypresses.")},
    Modifier{Qt::Key_Meta, kli18n("The Meta key has been locked and is now active for all of the following keypresses.")},
    Modifier{Qt::Key_AltGr, kli18n("The AltGr key has been locked and is now active for all of the following keypresses.")},
};

StickyKeysFilter::StickyKeysFilter()
    : InputEventFilter(InputFilterOrder::StickyKeys)
    , m_configWatcher(KConfigWatcher::create(KSharedConfig::openConfig("kaccessrc")))
{
    const QLatin1StringView groupName("Keyboard");
    connect(m_configWatcher.get(), &KConfigWatcher::configChanged, this, [this, groupName](const KConfigGroup &group) {
        if (group.name() == groupName) {
            loadConfig(group);
        }
    });
    loadConfig(m_configWatcher->config()->group(groupName));

    for (Qt::Key modifier : std::as_const(m_modifiers)) {
        m_keyStates[modifier] = KeyState::None;
    }
}

static Xkb::Modifier keyToModifier(Qt::Key key)
{
    if (key == Qt::Key_Shift) {
        return Xkb::Shift;
    } else if (key == Qt::Key_Alt) {
        return Xkb::Mod1;
    } else if (key == Qt::Key_Control) {
        return Xkb::Control;
    } else if (key == Qt::Key_AltGr) {
        return Xkb::Mod5;
    } else if (key == Qt::Key_Meta) {
        return Xkb::Mod4;
    }

    return Xkb::NoModifier;
}

void StickyKeysFilter::loadConfig(const KConfigGroup &group)
{
    input()->uninstallInputEventFilter(this);

    m_lockKeys = group.readEntry<bool>("StickyKeysLatch", true);
    m_showNotificationForLockedKeys = group.readEntry<bool>("kNotifyModifiers", false);
    m_disableOnTwoKeys = group.readEntry<bool>("StickyKeysAutoOff", false);
    m_ringBell = group.readEntry<bool>("StickyKeysBeep", false);

    bool changed = false;
    if (!m_lockKeys) {
        // locking keys is deactivated, unlock all locked keys
        for (auto [key, state] : m_keyStates.asKeyValueRange()) {
            if (state == KeyState::Locked) {
                state = KeyState::None;
                input()->keyboard()->xkb()->setModifierLocked(keyToModifier(key), false);
                changed = true;
            }
        }
    }

    if (group.readEntry<bool>("StickyKeys", false)) {
        input()->installInputEventFilter(this);
    } else {
        // sticky keys are deactivated, unlatch all latched/locked keys
        for (auto [key, state] : m_keyStates.asKeyValueRange()) {
            if (state != KeyState::None) {
                state = KeyState::None;
                input()->keyboard()->xkb()->setModifierLatched(keyToModifier(key), false);
                changed = true;
            }
        }
    }
    if (changed) {
        input()->keyboard()->xkb()->forwardModifiers();
        Q_EMIT input()->keyboard()->xkb()->modifierStateChanged();
    }
}

bool StickyKeysFilter::keyboardKey(KeyboardKeyEvent *event)
{
    const bool pressed = event->state == KeyboardKeyState::Repeated || event->state == KeyboardKeyState::Pressed;
    bool changed = false;

    if (m_modifiers.contains(event->key)) {
        if (pressed) {
            m_pressedModifiers << event->key;
        } else if (m_pressedModifiers.remove(event->key)) {
            if (m_ringBell) {
                if (auto effect = effects->provides(Effect::SystemBell)) {
                    effect->perform(Effect::SystemBell, {});
                }
            }

            if (auto keyState = m_keyStates.find(event->key); keyState != m_keyStates.end()) {
                // An unlatched modifier was pressed, latch it
                if (*keyState == KeyState::None) {
                    *keyState = KeyState::Latched;
                    input()->keyboard()->xkb()->setModifierLatched(keyToModifier(event->key), true);
                    changed = true;
                }
                // A latched modifier was pressed, lock it
                else if (*keyState == KeyState::Latched && m_lockKeys) {
                    *keyState = KeyState::Locked;
                    input()->keyboard()->xkb()->setModifierLatched(keyToModifier(event->key), false);
                    input()->keyboard()->xkb()->setModifierLocked(keyToModifier(event->key), true);
                    changed = true;

#if KWIN_BUILD_NOTIFICATIONS
                    if (m_showNotificationForLockedKeys) {
                        auto notification = new KNotification("modifierkey-locked");
                        notification->setComponentName("kaccess");

                        for (const auto &modifier : modifiers) {
                            if (modifier.key == event->key) {
                                notification->setText(modifier.lockedText.toString());
                                break;
                            }
                        }

                        notification->sendEvent();
                    }
#endif
                }
                // A locked modifier was pressed, unlock it
                else if (*keyState == KeyState::Locked && m_lockKeys) {
                    *keyState = KeyState::None;
                    input()->keyboard()->xkb()->setModifierLocked(keyToModifier(event->key), false);
                    changed = true;
                }
            }
        }
    } else if (pressed) {
        if (!m_pressedModifiers.isEmpty() && m_disableOnTwoKeys) {
            disableStickyKeys();
            changed = true;
        }

        // a non-modifier key was pressed, unlatch all unlocked modifiers
        for (auto [key, state] : m_keyStates.asKeyValueRange()) {
            if (state == KeyState::Locked) {
                continue;
            }

            state = KeyState::None;

            input()->keyboard()->xkb()->setModifierLatched(keyToModifier(key), false);
            changed = true;

            // Avoid latching and locking modifiers when a normal key is pressed so shortcuts work as expected.
            m_pressedModifiers.remove(key);
        }
    }
    if (changed) {
        Q_EMIT input()->keyboard()->xkb()->modifierStateChanged();
    }

    return false;
}

void StickyKeysFilter::disableStickyKeys()
{
    for (auto [key, state] : m_keyStates.asKeyValueRange()) {
        state = KeyState::None;
        input()->keyboard()->xkb()->setModifierLatched(keyToModifier(key), false);
        input()->keyboard()->xkb()->setModifierLocked(keyToModifier(key), false);
    }

    m_pressedModifiers.clear();

    input()->uninstallInputEventFilter(this);
}

bool StickyKeysFilter::pointerButton(PointerButtonEvent *event)
{
    if (event->state == PointerButtonState::Released) {
        // unlatch all unlocked modifiers
        for (auto [key, state] : m_keyStates.asKeyValueRange()) {
            if (state == KeyState::Locked) {
                continue;
            }

            state = KeyState::None;

            input()->keyboard()->xkb()->setModifierLatched(keyToModifier(key), false);

            // We need to delay the modifier update until the client received the mouse event, otherwise
            // the updated modifiers arrive before the mouse event and e.g. Ctrl+Click won't work
            QTimer::singleShot(0, this, [] {
                input()->keyboard()->xkb()->forwardModifiers();
            });

            m_pressedModifiers.remove(key);
        }
    }

    return false;
}

}

#include "moc_stickykeys.cpp"
