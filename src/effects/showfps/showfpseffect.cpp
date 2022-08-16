/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "showfpseffect.h"

#include <QQmlContext>
#include <QWindow>

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

void ShowFpsEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    effects->prePaintScreen(data, presentTime);

    m_newFps += 1;

    m_paintDurationTimer.restart();
    m_paintAmount = 0;

    // detect highest monitor refresh rate
    int maximumFps = 0;
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
        m_window = std::make_unique<QWindow>();
        m_window->create();
        m_scene = std::make_unique<OffscreenQuickScene>(nullptr, m_window.get());
        const auto url = QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/showfps/qml/main.qml")));
        m_scene->setSource(url, {{QStringLiteral("effect"), QVariant::fromValue(this)}});
    }

    const auto rect = effects->renderTargetRect();
    m_scene->setGeometry(QRect(rect.x() + rect.width() - 300, 0, 300, 150));
}

void ShowFpsEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);

    auto now = std::chrono::steady_clock::now();
    if ((now - m_lastFpsTime) >= std::chrono::milliseconds(1000)) {
        m_fps = m_newFps;
        m_newFps = 0;
        m_lastFpsTime = now;
        Q_EMIT fpsChanged();
    }

    effects->renderOffscreenQuickView(m_scene.get());
}

void ShowFpsEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    effects->paintWindow(w, mask, region, data);

    // Take intersection of region and actual window's rect, minus the fps area
    //  (since we keep repainting it) and count the pixels.
    QRegion repaintRegion = region & w->frameGeometry().toRect();
    repaintRegion -= m_scene->geometry();
    for (const QRect &rect : repaintRegion) {
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
