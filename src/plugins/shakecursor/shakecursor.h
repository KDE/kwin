/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "input_event_spy.h"
#include "plugins/shakecursor/shakedetector.h"

#include <QTimer>
#include <QVariantAnimation>

namespace KWin
{

class Cursor;
class CursorItem;

class ShakeCursorEffect : public Effect, public InputEventSpy
{
    Q_OBJECT

public:
    ShakeCursorEffect();
    ~ShakeCursorEffect() override;

    static bool supported();

    void reconfigure(ReconfigureFlags flags) override;
    void pointerEvent(MouseEvent *event) override;
    bool isActive() const override;

private:
    void magnify(qreal magnification);

    QTimer m_resetCursorScaleTimer;
    QVariantAnimation m_resetCursorScaleAnimation;
    ShakeDetector m_shakeDetector;

    Cursor *m_cursor;
    std::unique_ptr<CursorItem> m_cursorItem;
    qreal m_cursorMagnification = 1.0;
};

} // namespace KWin
