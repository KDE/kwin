/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "screenshothelper.h"
#include "effect/effecthandler.h"
#include "window.h"

#include <QDBusConnection>
#include <QTimer>

using namespace std::chrono_literals;

namespace KWin
{
ScreenshotHelper::ScreenshotHelper()
    : CrossFadeEffect()
{
    connect(effects, &EffectsHandler::windowAdded,
            this, [this](Window *window) {
        if (window->windowType() == WindowType::Spectacle) {
            if (m_spectacleWindows.isEmpty()) {
                start();
            }
            m_spectacleWindows.insert(window);
        }
    });
    connect(effects, &EffectsHandler::windowClosed,
            this, [this](Window *window) {
        m_spectacleWindows.remove(window);
        if (m_spectacleWindows.isEmpty()) {
            stop();
        }
    });
}

ScreenshotHelper::~ScreenshotHelper() = default;

bool ScreenshotHelper::supported()
{
    return effects->compositingType() == OpenGLCompositing && effects->animationsSupported();
}

void KWin::ScreenshotHelper::start()
{
    // int animationDuration = animationTime(400ms);

    if (!supported() || m_state != Off) {
        return;
    }
    if (effects->hasActiveFullScreenEffect()) {
        return;
    }

    const QList<EffectWindow *> allWindows = effects->stackingOrder();
    for (auto window : allWindows) {
        if (m_spectacleWindows.contains(window->window())) {
            continue;
        }
        redirect(window);
    }

    m_state = ShowingCache;
}

void ScreenshotHelper::stop()
{
    m_timeline.setDuration(std::chrono::milliseconds(50));
    effects->addRepaintFull();
    m_state = Blending;
}

bool ScreenshotHelper::isActive() const
{
    return m_state != Off;
}

void ScreenshotHelper::postPaintScreen()
{
    if (m_timeline.done()) {
        m_timeline.reset();
        m_state = Off;

        const QList<EffectWindow *> allWindows = effects->stackingOrder();
        for (auto window : allWindows) {
            unredirect(window);
        }
    }
    effects->addRepaintFull();
}

void ScreenshotHelper::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    data.setCrossFadeProgress(m_timeline.value());
    effects->paintWindow(renderTarget, viewport, w, mask, region, data);
}

void ScreenshotHelper::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (m_state == Off) {
        return;
    }
    if (m_state == Blending) {
        m_timeline.advance(presentTime);
    }

    effects->prePaintScreen(data, presentTime);
}

} // namespace KWin

#include "moc_screenshothelper.cpp"
