/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screencastsource.h"

namespace KWin
{

ScreenCastSource::ScreenCastSource()
{
}

bool ScreenCastSource::followsStreamSize()
{
    return false;
}

void ScreenCastSource::resize(const QSize &)
{
    Q_ASSERT(false);
}

} // namespace KWin

#include "moc_screencastsource.cpp"
