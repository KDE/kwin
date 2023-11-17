/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "effect/effect.h"

#include <KConfigWatcher>

class QTimer;
namespace KSvg
{
class Svg;
}

namespace KWin
{
class Glow;
class GLTexture;

class ScreenEdgeEffect : public Effect
{
    Q_OBJECT
public:
    ScreenEdgeEffect();
    ~ScreenEdgeEffect() override;
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 10;
    }

private Q_SLOTS:
    void edgeApproaching(ElectricBorder border, qreal factor, const QRect &geometry);
    void cleanup();

private:
    void ensureGlowSvg();
    std::unique_ptr<Glow> createGlow(ElectricBorder border, qreal factor, const QRect &geometry);
    QImage createCornerGlow(ElectricBorder border);
    QImage createEdgeGlow(ElectricBorder border, const QSize &size);
    QSize cornerGlowSize(ElectricBorder border);
    KConfigWatcher::Ptr m_configWatcher;
    KSvg::Svg *m_glow = nullptr;
    std::map<ElectricBorder, std::unique_ptr<Glow>> m_borders;
    QTimer *m_cleanupTimer;
};

class Glow
{
public:
    std::unique_ptr<GLTexture> texture;
    QImage image;
    QSize pictureSize;
    qreal strength;
    QRect geometry;
    ElectricBorder border;
};

}
