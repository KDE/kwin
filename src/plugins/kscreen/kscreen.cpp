/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "kscreen.h"
#include "core/output.h"
#include "effect/effecthandler.h"
#include "workspace.h"
// KConfigSkeleton
#include "kscreenconfig.h"

using namespace std::chrono_literals;

namespace KWin
{

KscreenEffect::KscreenEffect()
    : Effect()
{
    KscreenConfig::instance(effects->config());
    reconfigure(ReconfigureAll);

    connect(workspace(), &Workspace::dpmsStateChanged, this, &KscreenEffect::dpmsChanged);
    connect(effects, &EffectsHandler::screenRemoved, this, [this](LogicalOutput *screen) {
        m_states.remove(screen);
    });
}

void KscreenEffect::dpmsChanged(std::chrono::milliseconds animationTime)
{
    switch (workspace()->dpmsState()) {
    case Workspace::DpmsState::On: {
        const auto screens = effects->screens();
        for (LogicalOutput *screen : screens) {
            auto &state = m_states[screen];
            state.m_timeLine.setDuration(animationTime);
            setState(state, StateFadingIn);
        }
        break;
    }
    case Workspace::DpmsState::AboutToTurnOff: {
        const auto screens = effects->screens();
        for (LogicalOutput *screen : screens) {
            auto &state = m_states[screen];
            state.m_timeLine.setDuration(animationTime);
            setState(state, StateFadingOut);
        }
        break;
    }
    case Workspace::DpmsState::Off:
        break;
    }
}

void KscreenEffect::reconfigure(ReconfigureFlags flags)
{
    KscreenConfig::self()->read();
}

void KscreenEffect::prePaintScreen(ScreenPrePaintData &data)
{
    if (isScreenActive(data.screen)) {
        auto &state = m_states[data.screen];
        m_currentScreen = data.screen;

        if (state.m_state == StateFadingIn || state.m_state == StateFadingOut) {
            state.m_timeLine.advance(data.view);
            if (state.m_timeLine.done()) {
                switchState(state);
                if (state.m_state == StateNormal) {
                    m_states.remove(data.screen);
                }
            }
        }
    }

    effects->prePaintScreen(data);
}

void KscreenEffect::postPaintScreen()
{
    if (isScreenActive(m_currentScreen)) {
        auto &state = m_states[m_currentScreen];
        if (state.m_state == StateFadingIn || state.m_state == StateFadingOut) {
            effects->addRepaintFull();
        }
    }
    m_currentScreen = nullptr;
    effects->postPaintScreen();
}

void KscreenEffect::prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data)
{
    auto screen = w->screen();
    if (isScreenActive(screen)) {
        auto &state = m_states[screen];
        if (state.m_state != StateNormal) {
            data.setTranslucent();
        }
    }
    effects->prePaintWindow(view, w, data);
}

void KscreenEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    auto screen = w->screen();
    if (isScreenActive(screen)) {
        auto &state = m_states[screen];
        // fade to black and fully opaque
        switch (state.m_state) {
        case StateFadingOut:
            data.setOpacity(data.opacity() + (1.0 - data.opacity()) * state.m_timeLine.value());
            data.multiplyBrightness(1.0 - state.m_timeLine.value());
            break;
        case StateFadedOut:
            data.multiplyOpacity(0.0);
            data.multiplyBrightness(0.0);
            break;
        case StateFadingIn:
            data.setOpacity(data.opacity() + (1.0 - data.opacity()) * (1.0 - state.m_timeLine.value()));
            data.multiplyBrightness(state.m_timeLine.value());
            break;
        default:
            // no adjustment
            break;
        }
    }
    effects->paintWindow(renderTarget, viewport, w, mask, deviceRegion, data);
}

void KscreenEffect::setState(ScreenState &state, FadeOutState newState)
{
    if (state.m_state == newState) {
        return;
    }

    state.m_state = newState;
    state.m_timeLine.reset();
    effects->addRepaintFull();
}

void KscreenEffect::switchState(ScreenState &state)
{
    if (state.m_state == StateFadingOut) {
        state.m_state = StateFadedOut;
    } else if (state.m_state == StateFadingIn) {
        state.m_state = StateNormal;
    }
}

bool KscreenEffect::isActive() const
{
    return !m_states.isEmpty();
}

bool KscreenEffect::isScreenActive(LogicalOutput *screen) const
{
    return m_states.contains(screen);
}

} // namespace KWin

#include "moc_kscreen.cpp"
