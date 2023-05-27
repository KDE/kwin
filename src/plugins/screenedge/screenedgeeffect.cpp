/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screenedgeeffect.h"
// KWin
#include "libkwineffects/kwinoffscreenquickview.h"
#include "libkwineffects/rendertarget.h"
#include "libkwineffects/renderviewport.h"

#include <QTimer>
#include <QUrl>

namespace KWin
{

ScreenEdgeEffect::ScreenEdgeEffect()
    : Effect()
    , m_cleanupTimer(new QTimer(this))
{
    connect(effects, &EffectsHandler::screenEdgeApproaching, this, &ScreenEdgeEffect::edgeApproaching);
    m_cleanupTimer->setInterval(5000);
    m_cleanupTimer->setSingleShot(true);
    connect(m_cleanupTimer, &QTimer::timeout, this, &ScreenEdgeEffect::cleanup);
    connect(effects, &EffectsHandler::screenLockingChanged, this, [this](bool locked) {
        if (locked) {
            cleanup();
        }
    });
}

ScreenEdgeEffect::~ScreenEdgeEffect()
{
    cleanup();
}

void ScreenEdgeEffect::cleanup()
{
    // repaint screen here
    m_borders.clear();
}

void ScreenEdgeEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    effects->prePaintScreen(data, presentTime);
}

void ScreenEdgeEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, EffectScreen *screen)
{
    for (const auto &scene : m_borders) {
        effects->renderOffscreenQuickView(renderTarget, viewport, scene.second.get());
    }
}

void ScreenEdgeEffect::edgeApproaching(ElectricBorder border, qreal factor, const QRect &geometry)
{
    auto it = m_borders.find(border);
    if (it != m_borders.end()) {

        if (factor == 0.0) {
            m_cleanupTimer->start();
        } else {
            m_cleanupTimer->stop();
        }
    } else if (factor != 0.0) {
        // need to generate new Glow
        std::unique_ptr<OffscreenQuickScene> glow(new OffscreenQuickScene(nullptr));
        auto glowPtr = glow.get();

        connect(glow.get(), &OffscreenQuickView::repaintNeeded, this, [glowPtr] {
            effects->addRepaint(glowPtr->geometry());
        });
        //        connect(glow.get(), geometryChanged, this, [glowPtr](const QRect &old, const QRect &new) {
        //    });

        glow->setSource(QUrl::fromLocalFile("/home/david/edge.qml"));
        glow->setGeometry(QRect(0, 0, 30, 30));

        effects->addRepaint(glow->geometry());
        m_borders[border] = std::move(glow);
    }
}

bool ScreenEdgeEffect::isActive() const
{
    return !m_borders.empty() && !effects->isScreenLocked();
}

} // namespace
