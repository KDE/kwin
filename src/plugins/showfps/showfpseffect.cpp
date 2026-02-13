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

#include <QQmlContext>

namespace KWin
{

ShowFpsEffect::ShowFpsEffect()
{
}

ShowFpsEffect::~ShowFpsEffect() = default;

int ShowFpsEffect::fps() const
{
    return m_fps;
}

int ShowFpsEffect::maximumFps() const
{
    return m_maximumFps;
}

int ShowFpsEffect::paintDuration() const
{
    return m_paintDuration;
}

int ShowFpsEffect::paintAmount() const
{
    return m_paintAmount;
}

QColor ShowFpsEffect::paintColor() const
{
    auto normalizedDuration = std::min(1.0, m_paintDuration / 100.0);
    return QColor::fromHsvF(0.3 - (0.3 * normalizedDuration), 1.0, 1.0);
}

void ShowFpsEffect::prePaintScreen(ScreenPrePaintData &data)
{
    effects->prePaintScreen(data);

    m_newFps += 1;

    m_paintDurationTimer.restart();
    m_paintAmount = 0;

    // detect highest monitor refresh rate
    uint32_t maximumFps = 0;
    const auto screens = effects->screens();
    for (auto screen : screens) {
        maximumFps = std::max(screen->refreshRate(), maximumFps);
    }
    maximumFps /= 1000; // Convert from mHz to Hz.

    if (maximumFps != m_maximumFps) {
        m_maximumFps = maximumFps;
        Q_EMIT maximumFpsChanged();
    }

    if (!m_scene) {
        m_scene = std::make_unique<OffscreenQuickScene>();
        m_scene->loadFromModule(QStringLiteral("org.kde.kwin.showfps"), QStringLiteral("Main"), {{QStringLiteral("effect"), QVariant::fromValue(this)}});
        if (!m_scene->rootItem()) {
            // main-fallback.qml has less dependencies than main.qml, so it should work on any system where kwin compiles
            m_scene->loadFromModule(QStringLiteral("org.kde.kwin.showfps"), QStringLiteral("Fallback"), {{QStringLiteral("effect"), QVariant::fromValue(this)}});
        }
    }
}

void ShowFpsEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen)
{
    effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);

    auto now = std::chrono::steady_clock::now();
    if ((now - m_lastFpsTime) >= std::chrono::milliseconds(1000)) {
        m_fps = m_newFps;
        m_newFps = 0;
        m_lastFpsTime = now;
        Q_EMIT fpsChanged();
    }

    const auto rect = viewport.renderRect();
    m_scene->setGeometry(QRect(rect.x() + rect.width() - 300, rect.y(), 300, 150));
    effects->renderOffscreenQuickView(renderTarget, viewport, m_scene.get());
}

void ShowFpsEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    effects->paintWindow(renderTarget, viewport, w, mask, deviceRegion, data);

    // Take intersection of region and actual window's rect, minus the fps area
    //  (since we keep repainting it) and count the pixels.
    Region repaintRegion = deviceRegion & viewport.mapToDeviceCoordinatesAligned(w->frameGeometry());
    repaintRegion -= viewport.mapToDeviceCoordinatesAligned(Rect(m_scene->geometry()));
    for (const Rect &rect : repaintRegion.rects()) {
        m_paintAmount += rect.width() * rect.height();
    }
}

void ShowFpsEffect::postPaintScreen()
{
    effects->postPaintScreen();

    m_paintDuration = m_paintDurationTimer.elapsed();
    Q_EMIT paintChanged();

    effects->addRepaint(m_scene->geometry());
}

bool ShowFpsEffect::supported()
{
    return effects->isOpenGLCompositing();
}

} // namespace KWin

#include "moc_showfpseffect.cpp"
