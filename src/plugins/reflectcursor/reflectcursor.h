/*
    SPDX-FileCopyrightText: 2024 Yifan Zhu <fanzhuyifan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/output.h"
#include "effect/effect.h"
#include "input_event_spy.h"

namespace KWin
{

class Cursor;
class CursorItem;

class ReflectCursorEffect : public Effect, public InputEventSpy
{
    Q_OBJECT

public:
    ReflectCursorEffect();
    ~ReflectCursorEffect() override;

    static bool supported();

    void reconfigure(ReconfigureFlags flags) override;
    void pointerEvent(MouseEvent *event) override;
    bool isActive() const override;

private:
    void showEffect();
    void hideEffect();

    void update(const QRectF &cursorGeometry, const QPointF &hotspot, const QRectF &screenGeometry);

    Cursor *m_cursor;
    std::unique_ptr<CursorItem> m_cursorItem;

    bool m_reflectX = false;
    bool m_reflectY = false;

    // Consider reflecting the cursor when proportion of area off-screen is more than this
    qreal m_threshold = 0.3;
};

} // namespace KWin
