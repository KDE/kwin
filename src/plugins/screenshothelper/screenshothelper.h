/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "effect/offscreeneffect.h"
#include "effect/timeline.h"
#include "window.h"
#include <chrono>

namespace KWin
{

class ScreenshotHelper : public CrossFadeEffect
{
    Q_OBJECT

public:
    ScreenshotHelper();
    ~ScreenshotHelper() override;

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
     * Called from DBus
     */
    void start();
    void stop();
    void showFrozenMode(bool show);

private:
    enum State {
        Off,
        Captured, // screenshot taken, but showing live
        ShowingCache, // showing the screenshot
    };
    State m_state = Off;
    QSet<Window *> m_spectacleWindows;
};

} // namespace KWin
