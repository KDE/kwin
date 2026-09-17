/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/precisetimer.h"

#include <memory>

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
    static std::unique_ptr<VsyncSource> create();

    int refreshRate() const;
    void setRefreshRate(int refreshRate);

public Q_SLOTS:
    void arm();
    void disarm();

Q_SIGNALS:
    void vblankOccurred(std::chrono::nanoseconds timestamp);

private:
    explicit VsyncSource();
    void handleSyntheticVsync();

    PreciseTimer m_softwareClock;
    int m_refreshRate = 60000;
    std::chrono::nanoseconds m_vblankTimestamp = std::chrono::nanoseconds::zero();
};

} // namespace KWin
