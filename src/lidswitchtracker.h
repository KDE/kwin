/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "input_event_spy.h"

#include <QObject>

namespace KWin
{

class LidSwitchTracker : public QObject, InputEventSpy
{
    Q_OBJECT
public:
    explicit LidSwitchTracker();

    bool isLidClosed() const;

Q_SIGNALS:
    void lidStateChanged();

private:
    void switchEvent(KWin::SwitchEvent *event) override;

    bool m_isLidClosed = false;
};

}
