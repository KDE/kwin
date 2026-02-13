/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "blendchanges.h"
#include "effect/effecthandler.h"
#include "opengl/glutils.h"

#include <QDBusConnection>
#include <QTimer>

using namespace std::chrono_literals;

namespace KWin
{
BlendChanges::BlendChanges()
    : CrossFadeEffect()
{
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/KWin/BlendChanges"),
                                                 QStringLiteral("org.kde.KWin.BlendChanges"),
                                                 this,
                                                 QDBusConnection::ExportAllSlots);

    m_timeline.setEasingCurve(QEasingCurve::InOutCubic);
}

BlendChanges::~BlendChanges() = default;

bool BlendChanges::supported()
{
    return effects->compositingType() == OpenGLCompositing && effects->animationsSupported();
}

void KWin::BlendChanges::start(int delay)
{
    int animationDuration = animationTime(400ms);

    if (!supported() || m_state != Off) {
        return;
    }
    if (effects->hasActiveFullScreenEffect()) {
        return;
    }

    const QList<EffectWindow *> allWindows = effects->stackingOrder();
    for (auto window : allWindows) {
        if (!window->isFullScreen()) {
            redirect(window);
        }
    }

    QTimer::singleShot(delay, this, [this, animationDuration]() {
        m_timeline.setDuration(std::chrono::milliseconds(animationDuration));
        effects->addRepaintFull();
        m_state = Blending;
    });

    m_state = ShowingCache;
}

bool BlendChanges::isActive() const
{
    return m_state != Off;
}

void BlendChanges::postPaintScreen()
{
    if (m_timeline.done()) {
        m_timeline.reset();
        m_state = Off;

        const QList<EffectWindow *> allWindows = effects->stackingOrder();
        for (auto window : allWindows) {
            unredirect(window);
        }
    }
    effects->addRepaintFull();
}

void BlendChanges::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    data.setCrossFadeProgress(m_timeline.value());
    effects->paintWindow(renderTarget, viewport, w, mask, deviceRegion, data);
}

void BlendChanges::prePaintScreen(ScreenPrePaintData &data)
{
    if (m_state == Off) {
        return;
    }
    if (m_state == Blending) {
        m_timeline.advance(data.view);
    }

    effects->prePaintScreen(data);
}

} // namespace KWin

#include "moc_blendchanges.cpp"
