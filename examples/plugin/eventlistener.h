/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "kwin/plugin.h"
#include "kwin/input_event_spy.h"

namespace KWin
{

class EventListener : public Plugin, public InputEventSpy
{
    Q_OBJECT

public:
    EventListener();

    void keyEvent(KeyEvent *event) override;
    void pointerEvent(MouseEvent *event) override;
};

} // namespace KWin
