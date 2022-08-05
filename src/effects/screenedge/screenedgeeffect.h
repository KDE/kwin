/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCREEN_EDGE_EFFECT_H
#define KWIN_SCREEN_EDGE_EFFECT_H
#include <kwineffects.h>

class QTimer;
class QWindow;

namespace KWin
{
class OffscreenQuickScene;

class ScreenEdgeEffect : public Effect
{
    Q_OBJECT
public:
    ScreenEdgeEffect();
    ~ScreenEdgeEffect() override;
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 10;
    }

private Q_SLOTS:
    void edgeApproaching(ElectricBorder border, qreal factor, const QRect &geometry);
    void cleanup();

private:
    std::unique_ptr<OffscreenQuickScene> createGlow(ElectricBorder border, qreal factor, const QRect &geometry);

    std::map<ElectricBorder, std::unique_ptr<OffscreenQuickScene>> m_borders;
    std::unique_ptr<QWindow> m_dummyWindow;
    QTimer *m_cleanupTimer;
};

}

#endif
