/*
    SPDX-FileCopyrightText: 2022 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "stickykeys.h"
#include "keyboard_input.h"
#include "xkb.h"

StickyKeysFilter::StickyKeysFilter()
    : m_configWatcher(KConfigWatcher::create(KSharedConfig::openConfig("kaccessrc")))
{
    const QLatin1String groupName("Keyboard");
    connect(m_configWatcher.get(), &KConfigWatcher::configChanged, this, [this, groupName](const KConfigGroup &group) {
        if (group.name() == groupName) {
            loadConfig(group);
        }
    });
    loadConfig(m_configWatcher->config()->group(groupName));

    for (int mod : std::as_const(m_modifiers)) {
        m_keyStates[mod] = None;
    }
}

Qt::KeyboardModifier keyToModifier(Qt::Key key)
{
    if (key == Qt::Key_Shift) {
        return Qt::ShiftModifier;
    } else if (key == Qt::Key_Alt) {
        return Qt::AltModifier;
    } else if (key == Qt::Key_Control) {
        return Qt::ControlModifier;
    } else if (key == Qt::Key_AltGr) {
        return Qt::GroupSwitchModifier;
    } else if (key == Qt::Key_Meta) {
        return Qt::MetaModifier;
    }

    return Qt::NoModifier;
}

void StickyKeysFilter::loadConfig(const KConfigGroup &group)
{
    KWin::input()->uninstallInputEventFilter(this);

    m_lockKeys = group.readEntry<bool>("StickyKeysLatch", true);

    if (!m_lockKeys) {
        // locking keys is deactivated, unlock all locked keys
        for (auto it = m_keyStates.keyValueBegin(); it != m_keyStates.keyValueEnd(); ++it) {
            if (it->second == KeyState::Locked) {
                it->second = KeyState::None;
                KWin::input()->keyboard()->xkb()->setModifierLocked(keyToModifier(static_cast<Qt::Key>(it->first)), false);
            }
        }
    }

    if (group.readEntry<bool>("StickyKeys", false)) {
        KWin::input()->prependInputEventFilter(this);
    } else {
        // sticky keys are deactivated, unlatch all latched keys
        for (auto it = m_keyStates.keyValueBegin(); it != m_keyStates.keyValueEnd(); ++it) {
            if (it->second != KeyState::None) {
                it->second = KeyState::None;
                KWin::input()->keyboard()->xkb()->setModifierLatched(keyToModifier(static_cast<Qt::Key>(it->first)), false);
            }
        }
    }
}

bool StickyKeysFilter::keyEvent(KWin::KeyEvent *event)
{
    if (m_modifiers.contains(event->key())) {

        auto keyState = m_keyStates.find(event->key());

        if (keyState != m_keyStates.end()) {
            if (event->type() == QKeyEvent::KeyPress) {
                // An unlatched modifier was pressed, latch it
                if (keyState.value() == None) {
                    keyState.value() = Latched;
                    KWin::input()->keyboard()->xkb()->setModifierLatched(keyToModifier(static_cast<Qt::Key>(event->key())), true);
                }
                // A latched modifier was pressed, lock it
                else if (keyState.value() == Latched && m_lockKeys) {
                    keyState.value() = Locked;
                    KWin::input()->keyboard()->xkb()->setModifierLocked(keyToModifier(static_cast<Qt::Key>(event->key())), true);
                }
                // A locked modifier was pressed, unlock it
                else if (keyState.value() == Locked && m_lockKeys) {
                    keyState.value() = None;
                    KWin::input()->keyboard()->xkb()->setModifierLocked(keyToModifier(static_cast<Qt::Key>(event->key())), false);
                }
            }
        }
    } else if (event->type() == QKeyEvent::KeyPress) {
        // a non-modifier key was pressed, unlatch all unlocked modifiers
        for (auto it = m_keyStates.keyValueBegin(); it != m_keyStates.keyValueEnd(); ++it) {

            if (it->second == Locked) {
                continue;
            }

            it->second = KeyState::None;

            KWin::input()->keyboard()->xkb()->setModifierLatched(keyToModifier(static_cast<Qt::Key>(it->first)), false);
        }
    }

    return false;
}

#include "moc_stickykeys.cpp"
