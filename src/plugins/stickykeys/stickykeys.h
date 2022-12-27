/*
    SPDX-FileCopyrightText: 2022 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "plugin.h"

#include "input.h"
#include "input_event.h"

class StickyKeysFilter : public KWin::Plugin, public KWin::InputEventFilter
{
    Q_OBJECT
public:
    explicit StickyKeysFilter();

    bool keyEvent(KWin::KeyEvent *event) override;

    enum KeyState {
        None,
        Latched,
    };

private:
    void loadConfig(const KConfigGroup &group);

    KConfigWatcher::Ptr m_configWatcher;
    QMap<int, KeyState> m_keyStates;
    QVector<int> m_modifiers = {Qt::Key_Shift, Qt::Key_Control, Qt::Key_Alt, Qt::Key_AltGr, Qt::Key_Meta};
};
