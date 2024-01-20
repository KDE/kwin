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

class ImageItem;

class ScreenEdgeEffect : public Effect
{
    Q_OBJECT
public:
    ScreenEdgeEffect();
    ~ScreenEdgeEffect() override;
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
    QImage cornerGlowImage(ElectricBorder border);
    QImage edgeGlowImage(ElectricBorder border, const QSize &size);
    QImage glowImage(ElectricBorder border, const QSize &size);
    std::unique_ptr<ImageItem> createGlowItem(ElectricBorder border, qreal factor, const QRect &geometry);

    KConfigWatcher::Ptr m_configWatcher;
    KSvg::Svg *m_glow = nullptr;
    std::unordered_map<ElectricBorder, std::unique_ptr<ImageItem>> m_glowItems;
    QTimer *m_cleanupTimer;
};

} // namespace KWin
