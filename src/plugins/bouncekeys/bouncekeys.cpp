/*
    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "bouncekeys.h"
#include "keyboard_input.h"

BounceKeysFilter::BounceKeysFilter()
    : m_configWatcher(KConfigWatcher::create(KSharedConfig::openConfig("kaccessrc")))
{
    const QLatin1String groupName("Keyboard");
    connect(m_configWatcher.get(), &KConfigWatcher::configChanged, this, [this, groupName](const KConfigGroup &group) {
        if (group.name() == groupName) {
            loadConfig(group);
        }
    });
    loadConfig(m_configWatcher->config()->group(groupName));
}

void BounceKeysFilter::loadConfig(const KConfigGroup &group)
{
    KWin::input()->uninstallInputEventFilter(this);

    if (group.readEntry<bool>("BounceKeys", false)) {
        KWin::input()->prependInputEventFilter(this);

        m_delay = std::chrono::milliseconds(group.readEntry<int>("BounceKeysDelay", 500));
    } else {
        m_lastEvent.clear();
    }
}

bool BounceKeysFilter::keyEvent(KWin::KeyEvent *event)
{
    if (event->type() != KWin::KeyEvent::KeyPress) {
        return false;
    }

    auto it = m_lastEvent.find(event->key());

    if (it == m_lastEvent.end()) {
        // first time is always good
        m_lastEvent[event->key()] = event->timestamp();
        return false;
    }

    auto last = *it;
    *it = event->timestamp();

    return event->timestamp() - last < m_delay;
}

#include "moc_bouncekeys.cpp"
