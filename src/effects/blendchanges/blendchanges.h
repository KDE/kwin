/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include <chrono>
#include <kwindeformeffect.h>
#include <kwineffects.h>

namespace KWin
{

class BlendChanges : public DeformEffect
{
    Q_OBJECT

public:
    BlendChanges();
    ~BlendChanges() override;

    static bool supported();

    // Effect interface
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void postPaintScreen() override;
    void drawWindow(EffectWindow *window, int mask, const QRegion &region, WindowPaintData &data) override;
    void deform(EffectWindow *window, int mask, WindowPaintData &data, WindowQuadList &quads) override;
    bool isActive() const override;

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
