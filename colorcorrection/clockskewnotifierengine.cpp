/*
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "clockskewnotifierengine_p.h"
#if defined(Q_OS_LINUX)
#include "clockskewnotifierengine_linux.h"
#endif

namespace KWin
{

ClockSkewNotifierEngine *ClockSkewNotifierEngine::create(QObject *parent)
{
#if defined(Q_OS_LINUX)
    return LinuxClockSkewNotifierEngine::create(parent);
#else
    return nullptr;
#endif
}

ClockSkewNotifierEngine::ClockSkewNotifierEngine(QObject *parent)
    : QObject(parent)
{
}

} // namespace KWin
