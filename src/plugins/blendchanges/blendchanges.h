/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "effect/offscreeneffect.h"
#include "effect/timeline.h"
#include <chrono>

namespace KWin
{

class BlendChanges : public CrossFadeEffect
{
    Q_OBJECT

public:
    BlendChanges();
    ~BlendChanges() override;

    static bool supported();

    // Effect interface
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void postPaintScreen() override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 80;
    }

public Q_SLOTS:
    /**
     * Called from DBus, this should be called before triggering any changes
     * delay (ms) refers to how long to keep the current frame before starting a crossfade
     * We should expect all clients to have repainted by the time this expires
     */
    void start(int delay = 300);

private:
    TimeLine m_timeline;
    enum State {
        Off,
        ShowingCache,
        Blending
    };
    State m_state = Off;
};

} // namespace KWin
