/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "showfpseffect.h"
#include "core/output.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "scene/workspacescene.h"

#include <QQmlContext>

namespace KWin
{

ShowFpsEffect::ShowFpsEffect()
{
    connect(effects->scene(), &WorkspaceScene::viewRemoved, this, &ShowFpsEffect::removeView);
}

ShowFpsEffect::~ShowFpsEffect()
{
}

void ShowFpsEffect::removeView(RenderView *view)
{
    m_data.erase(view);
}

int ShowFpsScreen::fps() const
{
    return m_fps;
}

int ShowFpsScreen::maximumFps() const
{
    return m_maximumFps;
}

int ShowFpsScreen::paintDuration() const
{
    return m_paintDuration;
}

int ShowFpsScreen::paintAmount() const
{
    return m_paintAmount;
}

QColor ShowFpsScreen::paintColor() const
{
    auto normalizedDuration = std::min(1.0, m_paintDuration / 100.0);
    return QColor::fromHsvF(0.3 - (0.3 * normalizedDuration), 1.0, 1.0);
}

void ShowFpsEffect::prePaintScreen(ScreenPrePaintData &data)
{
    effects->prePaintScreen(data);

    auto &screenData = m_data[data.view];
    m_currentView = data.view;
    if (!screenData) {
        screenData = std::make_unique<ShowFpsScreen>();
    }

    screenData->m_newFps += 1;

    screenData->m_paintDurationTimer.restart();
    screenData->m_paintAmount = 0;

    // detect highest monitor refresh rate
    uint32_t maximumFps = data.view->refreshRate() / 1000;

    if (maximumFps != screenData->m_maximumFps) {
        screenData->m_maximumFps = maximumFps;
        Q_EMIT screenData->maximumFpsChanged();
    }

    if (!screenData->m_scene) {
        screenData->m_scene = std::make_unique<OffscreenQuickScene>();
        screenData->m_scene->loadFromModule(QStringLiteral("org.kde.kwin.showfps"), QStringLiteral("Main"), {{QStringLiteral("effect"), QVariant::fromValue(screenData.get())}});
        if (!screenData->m_scene->rootItem()) {
            // main-fallback.qml has less dependencies than main.qml, so it should work on any system where kwin compiles
            screenData->m_scene->loadFromModule(QStringLiteral("org.kde.kwin.showfps"), QStringLiteral("Fallback"), {{QStringLiteral("effect"), QVariant::fromValue(screenData.get())}});
        }
    }
    auto now = std::chrono::steady_clock::now();
    if ((now - screenData->m_lastFpsTime) >= std::chrono::milliseconds(1000)) {
        screenData->m_fps = screenData->m_newFps;
        screenData->m_newFps = 0;
        screenData->m_lastFpsTime = now;
        Q_EMIT screenData->fpsChanged();
    }

    const auto rect = data.view->viewport();
    screenData->m_scene->setGeometry(QRect(rect.x() + rect.width() - 300, rect.y(), 300, 150));
}

void ShowFpsEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    effects->paintWindow(renderTarget, viewport, w, mask, deviceRegion, data);

    auto &screenData = m_data[m_currentView];
    // Take intersection of region and actual window's rect, minus the fps area
    //  (since we keep repainting it) and count the pixels.
    Region repaintRegion = deviceRegion & viewport.mapToDeviceCoordinatesAligned(w->frameGeometry());
    repaintRegion -= viewport.mapToDeviceCoordinatesAligned(Rect(screenData->m_scene->geometry()));
    for (const Rect &rect : repaintRegion.rects()) {
        screenData->m_paintAmount += rect.width() * rect.height();
    }
}

void ShowFpsEffect::postPaintScreen()
{
    effects->postPaintScreen();

    auto &screenData = m_data[m_currentView];
    screenData->m_paintDuration = screenData->m_paintDurationTimer.elapsed();
    Q_EMIT screenData->paintChanged();
}

bool ShowFpsEffect::supported()
{
    return effects->isOpenGLCompositing();
}

} // namespace KWin

#include "moc_showfpseffect.cpp"
