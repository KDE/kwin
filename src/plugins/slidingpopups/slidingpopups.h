/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Marco Martin notmart @gmail.com
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "effect/effectwindow.h"
#include "effect/timeline.h"
#include "scene/item.h"

namespace KWin
{

class SlideManagerInterface;

class SlidingPopupsEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int slideInDuration READ slideInDuration)
    Q_PROPERTY(int slideOutDuration READ slideOutDuration)

public:
    SlidingPopupsEffect();
    ~SlidingPopupsEffect() override;

    void prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceGeometry, WindowPaintData &data) override;
    void reconfigure(ReconfigureFlags flags) override;
    bool isActive() const override;
    void postPaintScreen() override;

    int requestedEffectChainPosition() const override
    {
        return 40;
    }

    static bool supported();

    int slideInDuration() const;
    int slideOutDuration() const;

    bool eventFilter(QObject *watched, QEvent *event) override;
    bool blocksDirectScanout() const override;

private Q_SLOTS:
    void slotWindowAdded(EffectWindow *w);
    void slotWindowClosed(EffectWindow *w);
    void slotWindowDeleted(EffectWindow *w);
    void slotWaylandSlideOnShowChanged(EffectWindow *w);
    void slotWindowHiddenChanged(EffectWindow *w);

    void slideIn(EffectWindow *w);
    void slideOut(EffectWindow *w);
    void stopAnimations();

private:
    void setupAnimData(EffectWindow *w);
    void setupInternalWindowSlide(EffectWindow *w);
    void setupSlideData(EffectWindow *w);
    void setupInputPanelSlide();

    static SlideManagerInterface *s_slideManager;
    static QTimer *s_slideManagerRemoveTimer;

    int m_slideLength;
    std::chrono::milliseconds m_slideInDuration;
    std::chrono::milliseconds m_slideOutDuration;

    enum class AnimationKind {
        In,
        Out
    };

    struct Animation
    {
        EffectWindowDeletedRef deletedRef;
        EffectWindowVisibleRef visibleRef;
        AnimationKind kind;
        TimeLine timeLine;
        ItemEffect windowEffect;
    };
    std::unordered_map<EffectWindow *, Animation> m_animations;

    enum class Location {
        Left,
        Top,
        Right,
        Bottom
    };

    struct AnimationData
    {
        int offset;
        Location location;
        std::chrono::milliseconds slideInDuration;
        std::chrono::milliseconds slideOutDuration;
        int slideLength;
    };
    QHash<const EffectWindow *, AnimationData> m_animationsData;

    QRectF damagedLogicalArea(EffectWindow *w, const AnimationData animData);
};

inline int SlidingPopupsEffect::slideInDuration() const
{
    return m_slideInDuration.count();
}

inline int SlidingPopupsEffect::slideOutDuration() const
{
    return m_slideOutDuration.count();
}

} // namespace
