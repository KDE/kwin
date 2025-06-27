/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "effect/timeline.h"

namespace KWin
{

struct HighlightAnimation
{
    enum class State {
        Ghost,

        Highlighting,
        Highlighted,

        Withdrawing,
        Withdrawn,

        Reverting,
        Initial,
    };

    State state = State::Initial;
    TimeLine timeline;
    qreal initialOpacity = 1.0;
    qreal finalOpacity = 1.0;
    qreal opacity = 1.0;
};

class HighlightWindowEffect : public Effect
{
    Q_OBJECT

public:
    HighlightWindowEffect();
    ~HighlightWindowEffect() override;

    int requestedEffectChainPosition() const override
    {
        return 70;
    }

    bool provides(Feature feature) override;
    bool perform(Feature feature, const QVariantList &arguments) override;
    bool isActive() const override;
    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void postPaintScreen() override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    Q_SCRIPTABLE void highlightWindows(const QStringList &windows);

public Q_SLOTS:
    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);

private:
    void startGhostAnimation(EffectWindow *window);
    void startHighlightAnimation(EffectWindow *window);
    void startRevertAnimation(EffectWindow *window);

    bool isHighlighted(const EffectWindow *window) const;

    void prepareHighlighting();
    void finishHighlighting();
    void highlightWindows(const QList<KWin::EffectWindow *> &windows);

    QList<EffectWindow *> m_highlightedWindows;
    QHash<EffectWindow *, HighlightAnimation> m_animations;
    QEasingCurve m_easingCurve;
    std::chrono::milliseconds m_fadeDuration;
    float m_ghostOpacity = 0;
};

} // namespace
