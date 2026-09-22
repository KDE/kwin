/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "input_event_spy.h"
#include "plugins/shakecursor/shakedetector.h"
#include "scene/cursoritem.h"
#include "utils/cursortheme.h"

#include <QTimer>
#include <QVariantAnimation>

namespace KWin
{

class Cursor;
class CursorItem;
class ShapeCursorSource;
class GLTexture;
class GLFramebuffer;
class GLShader;

class ShakeCursorItem : public Item
{
    Q_OBJECT

public:
    ShakeCursorItem(const CursorTheme &theme, Item *parent);

private:
    void refresh();

    std::unique_ptr<ImageItem> m_imageItem;
    std::unique_ptr<ShapeCursorSource> m_source;
};

class ShakeCursorEffect : public Effect, public InputEventSpy
{
    Q_OBJECT

public:
    ShakeCursorEffect();
    ~ShakeCursorEffect() override;

    static bool supported();

    bool isActive() const override;
    void reconfigure(ReconfigureFlags flags) override;
    void pointerMotion(PointerMotionEvent *event) override;
    bool paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen) override;
    void postPaintScreen() override;

private:
    void magnify(qreal magnification);

    void inflate();
    void deflate();
    void animateTo(qreal magnification);

    QTimer m_deflateTimer;
    QVariantAnimation m_scaleAnimation;
    ShakeDetector m_shakeDetector;

    Cursor *m_cursor;
    std::unique_ptr<ShakeCursorItem> m_cursorItem;
    CursorTheme m_cursorTheme;
    qreal m_targetMagnification = 1.0;
    qreal m_currentMagnification = 1.0;

    bool m_useShader = false;
    bool m_resourcesInit = false;
    std::unique_ptr<GLTexture> m_offscreenTexture;
    std::unique_ptr<GLFramebuffer> m_offscreenFb;

    std::unique_ptr<GLTexture> m_positionsTexture;
    std::unique_ptr<GLFramebuffer> m_positionsFb;
    std::unique_ptr<GLShader> m_blackHoleShader;
    std::unique_ptr<GLShader> m_blackHolePhysicsShader;
    std::unique_ptr<GLShader> m_blackHoleInitShader;
    double m_blackHoleSize = 0;
    QPointF m_blackHolePosition;
    QPointF m_blackHoleStartPosition;
    double m_blackHoleStartMagnification = 0;
};

} // namespace KWin
