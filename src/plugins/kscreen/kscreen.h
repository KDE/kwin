/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "effect/effect.h"
#include "effect/timeline.h"

#include <xcb/xcb.h>

namespace KWin
{

class KscreenEffect : public Effect
{
    Q_OBJECT

public:
    KscreenEffect();
    ~KscreenEffect() override;

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void postPaintScreen() override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    void reconfigure(ReconfigureFlags flags) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 99;
    }

#if KWIN_BUILD_X11
private Q_SLOTS:
    void propertyNotify(KWin::EffectWindow *window, long atom);
#endif

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

    QHash<Output *, ScreenState> m_waylandStates;
    ScreenState m_xcbState;
    Output *m_currentScreen = nullptr;
#if KWIN_BUILD_X11
    xcb_atom_t m_atom;
#endif
};

} // namespace KWin
