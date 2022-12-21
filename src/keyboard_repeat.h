/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input_event_spy.h"

#include <QObject>

class QTimer;

namespace KWin
{
class Xkb;

class KeyboardRepeat : public QObject, public InputEventSpy
{
    Q_OBJECT
public:
    explicit KeyboardRepeat(Xkb *xkb);
    ~KeyboardRepeat() override;

    void keyEvent(KeyEvent *event) override;

Q_SIGNALS:
    void keyRepeat(quint32 key, std::chrono::microseconds time);

private:
    void handleKeyRepeat();
    QTimer *m_timer;
    Xkb *m_xkb;
    std::chrono::microseconds m_time;
    quint32 m_key = 0;
};

}
