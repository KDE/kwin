/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "effect/offscreenquickview.h"

#include <QElapsedTimer>

namespace KWin
{

class ShowFpsEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int fps READ fps NOTIFY fpsChanged)
    Q_PROPERTY(int maximumFps READ maximumFps NOTIFY maximumFpsChanged)
    Q_PROPERTY(int paintDuration READ paintDuration NOTIFY paintChanged)
    Q_PROPERTY(int paintAmount READ paintAmount NOTIFY paintChanged)
    Q_PROPERTY(QColor paintColor READ paintColor NOTIFY paintChanged)

public:
    ShowFpsEffect();
    ~ShowFpsEffect() override;

    int fps() const;
    int maximumFps() const;
    int paintDuration() const;
    int paintAmount() const;
    QColor paintColor() const;

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, QRegion region, KWin::WindowPaintData &data) override;
    void postPaintScreen() override;

    static bool supported();

Q_SIGNALS:
    void fpsChanged();
    void maximumFpsChanged();
    void paintChanged();

private:
    std::unique_ptr<OffscreenQuickScene> m_scene;

    uint32_t m_maximumFps = 0;

    int m_fps = 0;
    int m_newFps = 0;
    std::chrono::steady_clock::time_point m_lastFpsTime;

    int m_paintDuration = 0;
    int m_paintAmount = 0;
    QElapsedTimer m_paintDurationTimer;
};

} // namespace KWin
