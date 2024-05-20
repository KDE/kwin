/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/output.h"
#include "effect/crossfaderenderer.h"
#include "effect/effect.h"
#include "effect/screensnapshots.h"
#include "effect/timeline.h"

namespace KWin
{
class GLFramebuffer;
class GLShader;
class GLTexture;

class SessionFadeEffect : public Effect
{
    Q_OBJECT

public:
    SessionFadeEffect();
    ~SessionFadeEffect() override;

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, KWin::Output *screen) override;
    void prePaintWindow(KWin::EffectWindow *w, KWin::WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;

    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 99;
    }
    static bool supported();

public Q_SLOTS:
    void startFadeOut();
    void startFadeIn(bool fadeWithState);

private:
    void saveCurrentState();
    void removeScreen(Output *output);

    TimeLine m_timeLine;
    struct ScreenState
    {
        Snapshot m_prev;
        Snapshot m_current;
    };

    QHash<Output *, ScreenState> m_states;
    bool m_capturing = false;
    std::optional<std::chrono::milliseconds> m_firstPresent;
    CrossFadeRenderer m_crossfader;
};

} // namespace KWin
