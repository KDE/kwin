/*
    SPDX-FileCopyrightText: 2022 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "plugin.h"

#include "input.h"
#include "input_event.h"

namespace KWin
{

class StickyKeysFilter : public Plugin, public InputEventFilter
{
    Q_OBJECT

public:
    explicit StickyKeysFilter();

    bool keyboardKey(KeyboardKeyEvent *event) override;
    bool pointerButton(PointerButtonEvent *event) override;

    enum class KeyState {
        None,
        Latched,
        Locked,
    };

private:
    void loadConfig(const KConfigGroup &group);
    void disableStickyKeys();

    KConfigWatcher::Ptr m_configWatcher;
    QMap<Qt::Key, KeyState> m_modifierStates;
    QSet<Qt::Key> m_pressedModifiers;
    bool m_lockKeys = false;
    bool m_showNotificationForLockedKeys = false;
    bool m_disableOnTwoKeys = false;
    bool m_ringBell = false;
};

}
