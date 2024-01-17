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

private:
    void magnify(qreal magnification);

    void inflate();
    void deflate();
    void animateTo(qreal magnification);

    QTimer m_deflateTimer;
    QVariantAnimation m_scaleAnimation;
    ShakeDetector m_shakeDetector;

    Cursor *m_cursor;
    std::unique_ptr<CursorItem> m_cursorItem;
    qreal m_targetMagnification = 1.0;
    qreal m_currentMagnification = 1.0;
};

} // namespace KWin
