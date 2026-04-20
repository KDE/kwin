/*
    SPDX-FileCopyrightText: 2026 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "input_event_spy.h"
// #include "shakedetector.h"
#include "effect/effecttogglablestate.h"
#include "scene/cursoritem.h"
#include "utils/cursortheme.h"

namespace KWin
{

class Cursor;
class CursorItem;
class ShapeCursorSource;
class OffscreenQuickScene;

class MyCursorItem : public Item
{
    Q_OBJECT

public:
    MyCursorItem(const CursorTheme &theme, Item *parent);

private:
    void refresh();

    std::unique_ptr<ImageItem> m_imageItem;
    std::unique_ptr<ShapeCursorSource> m_source;
};

class MouseClickEffect2 : public Effect, public InputEventSpy
{
    Q_OBJECT
    Q_PROPERTY(QPointF cursorPos READ cursorPos NOTIFY cursorPosChanged FINAL)
    Q_PROPERTY(QPointF cursorHotSpot READ cursorHotSpot NOTIFY cursorHotSpotChanged FINAL)

public:
    MouseClickEffect2();
    ~MouseClickEffect2() override;

    static bool supported();
    QPointF cursorPos() const;
    QPointF cursorHotSpot() const;

    bool isActive() const override;
    void reconfigure(ReconfigureFlags flags) override;
    void pointerMotion(PointerMotionEvent *event) override;
    // void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen) override;

Q_SIGNALS:
    void cursorPosChanged();
    void cursorHotSpotChanged();

private:
    void setActive(bool active);

    void initOffscreenViews();
    void cleanupOffscreenViews();

    // ShakeDetector m_shakeDetector;

    bool m_active = false;
    EffectTogglableState *const m_effectState;
    Cursor *m_cursor;
    std::unique_ptr<MyCursorItem> m_cursorItem;
    CursorTheme m_cursorTheme;
    std::unordered_map<LogicalOutput *, std::unique_ptr<OffscreenQuickScene>> m_scenesByScreens;
};

}
