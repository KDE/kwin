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

class RenderView;

class ShowFpsScreen : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int fps READ fps NOTIFY fpsChanged)
    Q_PROPERTY(int maximumFps READ maximumFps NOTIFY maximumFpsChanged)
    Q_PROPERTY(int paintDuration READ paintDuration NOTIFY paintChanged)
    Q_PROPERTY(int paintAmount READ paintAmount NOTIFY paintChanged)
    Q_PROPERTY(QColor paintColor READ paintColor NOTIFY paintChanged)

public:
    int fps() const;
    int maximumFps() const;
    int paintDuration() const;
    int paintAmount() const;
    QColor paintColor() const;

Q_SIGNALS:
    void fpsChanged();
    void maximumFpsChanged();
    void paintChanged();

public:
    std::unique_ptr<OffscreenQuickScene> m_scene;
    int m_fps = 0;
    int m_newFps = 0;
    uint32_t m_maximumFps = 0;
    std::chrono::steady_clock::time_point m_lastFpsTime;
    int m_paintDuration = 0;
    int m_paintAmount = 0;
    QElapsedTimer m_paintDurationTimer;
};

class ShowFpsEffect : public Effect
{
    Q_OBJECT

public:
    ShowFpsEffect();
    ~ShowFpsEffect() override;

    void prePaintScreen(ScreenPrePaintData &data) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, KWin::WindowPaintData &data) override;
    void postPaintScreen() override;

    static bool supported();

private:
    void removeView(RenderView *view);

    std::unordered_map<RenderView *, std::unique_ptr<ShowFpsScreen>> m_data;
    RenderView *m_currentView = nullptr;
};

} // namespace KWin
