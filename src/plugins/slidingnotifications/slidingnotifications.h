/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "effect/effecthandler.h"
#include "effect/springmotion.h"
#include "effect/timeline.h"
#include "scene/item.h"

namespace KWin
{

class SlideNotificationAnimation
{
public:
    explicit SlideNotificationAnimation(EffectWindow *window);

    void revert();
    void advance(RenderView *view);
    bool done() const;

    QPointF offset() const;
    qreal opacity() const;

    RectF clipArea() const;
    RectF dirtyArea() const;

    void apply(WindowPaintData &data);

    static std::unique_ptr<SlideNotificationAnimation> intro(EffectWindow *window);
    static std::unique_ptr<SlideNotificationAnimation> outro(EffectWindow *window);

    EffectWindow *window;
    SpringMotion fade; // opacity spring, which is stiffer than the motion springs so it settles first
    AnimationClock clock;
    SpringMotion springX;
    SpringMotion springY;
    QPointF slideTarget;
};

class DisplaceNotificationAnimation
{
public:
    explicit DisplaceNotificationAnimation(EffectWindow *window);

    RectF dirtyArea() const;

    void moveTo(const QPointF &point);
    void advance(RenderView *view);
    bool done() const;

    QPointF position() const;

    void apply(WindowPaintData &data);

    static std::unique_ptr<DisplaceNotificationAnimation> move(EffectWindow *window, const QPointF &initialPosition, const QPointF &finalPosition);

    EffectWindow *window;
    AnimationClock clock;
    SpringMotion springX;
    SpringMotion springY;
};

class NotificationAnimation
{
public:
    explicit NotificationAnimation(EffectWindow *window);
    ~NotificationAnimation();

    bool isFinished() const;
    void advance(RenderView *view);
    RectF dirtyArea() const;

    EffectWindow *window;
    ItemEffect effect;
    EffectWindowDeletedRef deletedRef;

    std::unique_ptr<SlideNotificationAnimation> slide;
    std::unique_ptr<DisplaceNotificationAnimation> displace;
};

class SlidingNotificationsEffect : public Effect
{
    Q_OBJECT

public:
    explicit SlidingNotificationsEffect();

    bool isActive() const override;
    int requestedEffectChainPosition() const override;
    bool blocksDirectScanout() const override;

    void prePaintWindow(RenderView *view, EffectWindow *window, WindowPrePaintData &data) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceGeometry, WindowPaintData &data) override;
    void postPaintScreen() override;

    static bool supported();

private:
    void onWindowAdded(EffectWindow *window);
    void onWindowClosed(EffectWindow *window);
    void onWindowFrameGeometryChanged(EffectWindow *window, const RectF &oldGeometry);

    std::unordered_map<EffectWindow *, std::unique_ptr<NotificationAnimation>> m_animations;
};

}
