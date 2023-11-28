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
class GLTexture;

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
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen) override;

private:
    GLTexture *ensureCursorTexture();
    void markCursorTextureDirty();

    void showCursor();
    void hideCursor();

    struct Transaction
    {
        QPointF position;
        QPointF hotspot;
        QSizeF size;
        qreal magnification;
        bool damaged = false;
    };
    void update(const Transaction &transaction);

    QTimer m_resetCursorScaleTimer;
    QVariantAnimation m_resetCursorScaleAnimation;
    ShakeDetector m_shakeDetector;

    Cursor *m_cursor;
    QRectF m_cursorGeometry;
    qreal m_cursorMagnification = 1.0;

    std::unique_ptr<GLTexture> m_cursorTexture;
    bool m_cursorTextureDirty = false;
    bool m_mouseHidden = false;
};

} // namespace KWin
