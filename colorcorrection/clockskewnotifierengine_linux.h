/*
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "clockskewnotifierengine_p.h"

namespace KWin
{

class LinuxClockSkewNotifierEngine : public ClockSkewNotifierEngine
{
    Q_OBJECT

public:
    ~LinuxClockSkewNotifierEngine() override;

    static LinuxClockSkewNotifierEngine *create(QObject *parent);

private Q_SLOTS:
    void handleTimerCancelled();

private:
    LinuxClockSkewNotifierEngine(int fd, QObject *parent);

    int m_fd;
};

} // namespace KWin
