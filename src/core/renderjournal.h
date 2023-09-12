/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "libkwineffects/kwinglobals.h"

#include <QElapsedTimer>
#include <QQueue>

namespace KWin
{

/**
 * The RenderJournal class measures how long it takes to render frames and estimates how
 * long it will take to render the next frame.
 */
class KWIN_EXPORT RenderJournal
{
public:
    RenderJournal();

    void add(std::chrono::nanoseconds renderTime);

    std::chrono::nanoseconds result() const;

private:
    std::chrono::nanoseconds m_result{0};
    std::optional<std::chrono::steady_clock::time_point> m_lastAdd;
};

} // namespace KWin
