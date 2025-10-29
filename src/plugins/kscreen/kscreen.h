/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "effect/effect.h"
#include "effect/timeline.h"

namespace KWin
{

class KscreenEffect : public Effect
{
    Q_OBJECT

public:
    KscreenEffect();

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void postPaintScreen() override;
    void prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &deviceRegion, WindowPaintData &data) override;

    void reconfigure(ReconfigureFlags flags) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 99;
    }

private:
    enum FadeOutState {
        StateNormal,
        StateFadingOut,
        StateFadedOut,
        StateFadingIn,
        LastState
    };
    struct ScreenState
    {
        TimeLine m_timeLine;
        FadeOutState m_state = StateNormal;
    };

    void switchState(ScreenState &state);
    void setState(ScreenState &state, FadeOutState newState);
    void addScreen(Output *screen);
    bool isScreenActive(Output *screen) const;

    QHash<Output *, ScreenState> m_states;
    Output *m_currentScreen = nullptr;
};

} // namespace KWin
