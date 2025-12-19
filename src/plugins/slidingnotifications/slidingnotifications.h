/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "effect/effecthandler.h"
#include "effect/timeline.h"

namespace KWin
{

class SlideNotificationAnimation
{
public:
    explicit SlideNotificationAnimation(EffectWindow *window);
    ~SlideNotificationAnimation();

    RectF clipArea() const;
    RectF dirtyArea() const;

    static std::unique_ptr<SlideNotificationAnimation> intro(EffectWindow *window);
    static std::unique_ptr<SlideNotificationAnimation> outro(EffectWindow *window);

    EffectWindow *window;
    EffectWindowDeletedRef deletedRef;
    TimeLine timeline;
    QPointF initialOffset;
    QPointF finalOffset;
    qreal initialOpacity;
    qreal finalOpacity;

private:
    Q_DISABLE_COPY(SlideNotificationAnimation)
};

class SlidingNotificationsEffect : public Effect
{
    Q_OBJECT

public:
    explicit SlidingNotificationsEffect();

    bool isActive() const override;
    int requestedEffectChainPosition() const override;

    void prePaintWindow(RenderView *view, EffectWindow *window, WindowPrePaintData &data) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceGeometry, WindowPaintData &data) override;
    void postPaintScreen() override;

    static bool supported();

private:
    void onWindowAdded(EffectWindow *window);
    void onWindowClosed(EffectWindow *window);

    std::unordered_multimap<EffectWindow *, std::unique_ptr<SlideNotificationAnimation>> m_animations;
};

} // namespace KWin
