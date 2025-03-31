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
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/ScreenshotHelper"), this, QDBusConnection::RegisterOption::ExportAllSlots);

    // connect(effects, &EffectsHandler::windowAdded,
    //         this, [this](EffectWindow *window) {
    //     if (window->window()->windowType() == WindowType::Spectacle) {
    //         m_spectacleWindows.insert(window);
    //     }
    // });
    // connect(effects, &EffectsHandler::windowClosed,
    //         this, [this](EffectWindow *window) {
    //     m_spectacleWindows.remove(window);

    // });

    // TODO show a little (draggable) QML UI on each window to pause/stop?
}

ScreenshotHelper::~ScreenshotHelper() = default;

bool ScreenshotHelper::supported()
{
    return effects->compositingType() == OpenGLCompositing && effects->animationsSupported();
}

void KWin::ScreenshotHelper::start()
{
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

    // TODO capture caller, and quit if that exits
    // or do the FD thing...?

    m_state = Captured;
}

void ScreenshotHelper::stop()
{
    effects->addRepaintFull();
    const QList<EffectWindow *> allWindows = effects->stackingOrder();
    for (auto window : allWindows) {
        unredirect(window);
    }
    m_state = Off;
}

void ScreenshotHelper::showFrozenMode(bool show)
{
    if (m_state == Off) {
        return;
    }
    m_state = show ? ShowingCache : Captured;
    effects->addRepaintFull();
}

bool ScreenshotHelper::isActive() const
{
    return m_state != Off;
}

void ScreenshotHelper::postPaintScreen()
{
    // potentially transition here
    // if (m_timeline.done()) {
    //     m_timeline.reset();
    //     m_state = Off;

    // }
    // effects->addRepaintFull();
}

void ScreenshotHelper::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    data.setCrossFadeProgress(m_state == Captured ? 1.0 : 0.0);
    effects->paintWindow(renderTarget, viewport, w, mask, region, data);
}

void ScreenshotHelper::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    // if (m_state == Off) {
    //     return;
    // }
    // for future transition
    // if (m_state == Blending) {
    //     m_timeline.advance(presentTime);
    // }

    effects->prePaintScreen(data, presentTime);
}

} // namespace KWin

#include "moc_screenshothelper.cpp"
