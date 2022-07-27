/*
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "clockskewnotifierengine_p.h"
#include "utils/filedescriptor.h"

namespace KWin
{

class LinuxClockSkewNotifierEngine : public ClockSkewNotifierEngine
{
    Q_OBJECT

public:
    static LinuxClockSkewNotifierEngine *create(QObject *parent);

private Q_SLOTS:
    void handleTimerCancelled();

private:
    LinuxClockSkewNotifierEngine(FileDescriptor &&fd, QObject *parent);

    FileDescriptor m_fd;
};

} // namespace KWin
