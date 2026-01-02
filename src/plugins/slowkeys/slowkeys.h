/*
    SPDX-FileCopyrightText: 2025 Martin Riethmayer <ripper@freakmail.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "plugin.h"

#include "input.h"
#include "input_event.h"
#include <chrono>

namespace KWin
{

class SlowKeysFilter : public Plugin, public InputEventFilter
{
    Q_OBJECT
public:
    explicit SlowKeysFilter();

    bool keyboardKey(KeyboardKeyEvent *event) override;

private:
    void loadConfig(const KConfigGroup &group);

    KConfigWatcher::Ptr m_configWatcher;
    std::chrono::milliseconds m_delay;
    bool m_keysPressBeep = false;
    bool m_keysAcceptBeep = false;
    bool m_keysRejectBeep = false;
    QHash<int, std::chrono::milliseconds> m_firstEvent;
};
}
