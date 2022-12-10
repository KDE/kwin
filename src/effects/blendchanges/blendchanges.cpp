/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "blendchanges.h"
#include "kwinglutils.h"

#include <QDBusConnection>
#include <QTimer>

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
    int animationDuration = animationTime(400);

    if (!supported() || m_state != Off) {
        return;
    }
    if (effects->hasActiveFullScreenEffect()) {
        return;
    }

    const EffectWindowList allWindows = effects->stackingOrder();
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

        const EffectWindowList allWindows = effects->stackingOrder();
        for (auto window : allWindows) {
            unredirect(window);
        }
    }
    effects->addRepaintFull();
}

void BlendChanges::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    data.setCrossFadeProgress(m_timeline.value());
    effects->paintWindow(w, mask, region, data);
}

void BlendChanges::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (m_state == Off) {
        return;
    }
    if (m_state == Blending) {
        m_timeline.advance(presentTime);
    }

    effects->prePaintScreen(data, presentTime);
}

} // namespace KWin
