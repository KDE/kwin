/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwinglobals.h"

#include <QObject>

#include <chrono>

namespace KWin
{

/**
 * The VsyncMonitor class provides a convenient way to monitor vblank events.
 */
class KWIN_EXPORT VsyncMonitor : public QObject
{
    Q_OBJECT

public:
    explicit VsyncMonitor();

public Q_SLOTS:
    virtual void arm() = 0;

Q_SIGNALS:
    void errorOccurred();
    void vblankOccurred(std::chrono::nanoseconds timestamp);
};

} // namespace KWin
