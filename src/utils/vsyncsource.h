/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/precisetimer.h"

namespace KWin
{

/**
 * The VsyncSource class provides synthetic vblank events with constant interval.
 *
 * The vblank interval can be changed by calling the setRefreshRate() function.
 */
class KWIN_EXPORT VsyncSource : public QObject
{
    Q_OBJECT

public:
    explicit VsyncSource();

    int refreshRate() const;
    void setRefreshRate(int refreshRate);

public Q_SLOTS:
    void arm();
    void disarm();

Q_SIGNALS:
    void vblankOccurred(std::chrono::steady_clock::time_point timestamp);

private:
    void handleSyntheticVsync();

    PreciseTimer m_softwareClock;
    int m_refreshRate = 60000;
    std::chrono::steady_clock::time_point m_vblankTimestamp{};
};

} // namespace KWin
