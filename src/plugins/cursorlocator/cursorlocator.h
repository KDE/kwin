/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "input_event_spy.h"
#include "plugin.h"

namespace KWin
{

class CursorLocator : public Plugin, public InputEventSpy
{
    Q_OBJECT

public:
    explicit CursorLocator();

    void pointerEvent(MouseEvent *event) override;

private:
    qreal m_pointerSpeed = 0;
    std::chrono::microseconds m_lastTimestamp;
};

} // namespace KWin
