/*
    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "plugin.h"

#include "input.h"
#include "input_event.h"

class BounceKeysFilter : public KWin::Plugin, public KWin::InputEventFilter
{
    Q_OBJECT
public:
    explicit BounceKeysFilter();

    bool keyEvent(KWin::KeyEvent *event) override;

private:
    void loadConfig(const KConfigGroup &group);

    KConfigWatcher::Ptr m_configWatcher;
    std::chrono::milliseconds m_delay;
    QHash<int, std::chrono::microseconds> m_lastEvent;
};
