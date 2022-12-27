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
        if (group.parent().name() == groupName) {
            loadConfig(group);
        }
    });
    loadConfig(m_configWatcher->config()->group(groupName));

    for (int mod : std::as_const(m_modifiers)) {
        m_keyStates[mod] = None;
    }
}

void StickyKeysFilter::loadConfig(const KConfigGroup &group)
{
    KWin::input()->uninstallInputEventFilter(this);

    if (group.readEntry<bool>("StickyKeys", false)) {
        KWin::input()->prependInputEventFilter(this);
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

bool StickyKeysFilter::keyEvent(KWin::KeyEvent *event)
{
    if (m_modifiers.contains(event->key())) {
        // A modifier was pressed, latch it
        if (event->type() == QKeyEvent::KeyPress && m_keyStates[event->key()] != Latched) {

            m_keyStates[event->key()] = Latched;

            KWin::input()->keyboard()->xkb()->setModifierLatched(keyToModifier((Qt::Key)event->key()), true);
        }
    } else if (event->type() == QKeyEvent::KeyPress) {
        // a non-modifier key was pressed, unlatch all modifiers
        for (auto it = m_keyStates.keyValueBegin(); it != m_keyStates.keyValueEnd(); ++it) {
            it->second = KeyState::None;

            KWin::input()->keyboard()->xkb()->setModifierLatched(keyToModifier((Qt::Key)it->first), false);
        }
    }

    return false;
}
