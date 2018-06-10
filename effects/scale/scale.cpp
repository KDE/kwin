/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

// own
#include "scale.h"

// KConfigSkeleton
#include "scaleconfig.h"

// Qt
#include <QSet>

namespace KWin
{

static const QSet<QString> s_blacklist {
    // The logout screen has to be animated only by the logout effect.
    QStringLiteral("ksmserver ksmserver"),

    // KDE Plasma splash screen has to be animated only by the login effect.
    QStringLiteral("ksplashqml ksplashqml"),
    QStringLiteral("ksplashsimple ksplashsimple"),
    QStringLiteral("ksplashx ksplashx")
};

ScaleEffect::ScaleEffect()
{
    initConfig<ScaleConfig>();
    reconfigure(ReconfigureAll);

    connect(effects, &EffectsHandler::windowAdded, this, &ScaleEffect::windowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &ScaleEffect::windowClosed);
    connect(effects, &EffectsHandler::windowDeleted, this, &ScaleEffect::windowDeleted);
    connect(effects, &EffectsHandler::windowDataChanged, this, &ScaleEffect::windowDataChanged);
}

ScaleEffect::~ScaleEffect()
{
}

void ScaleEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    ScaleConfig::self()->read();
    m_duration = std::chrono::milliseconds(animationTime<ScaleConfig>(160));

    m_inParams.scale.from = ScaleConfig::inScale();
    m_inParams.scale.to = 1.0;
    m_inParams.opacity.from = ScaleConfig::inOpacity();
    m_inParams.opacity.to = 1.0;

    m_outParams.scale.from = 1.0;
    m_outParams.scale.to = ScaleConfig::outScale();
    m_outParams.opacity.from = 1.0;
    m_outParams.opacity.to = ScaleConfig::outOpacity();
}

void ScaleEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    const std::chrono::milliseconds delta(time);

    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        (*animationIt).update(delta);
        ++animationIt;
    }

    effects->prePaintScreen(data, time);
}

void ScaleEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time)
{
    if (m_animations.contains(w)) {
        data.setTransformed();
        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DELETE);
    }

    effects->prePaintWindow(w, data, time);
}

void ScaleEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    auto animationIt = m_animations.constFind(w);
    if (animationIt == m_animations.constEnd()) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    const ScaleParams params = w->isDeleted() ? m_outParams : m_inParams;
    const qreal t = (*animationIt).value();
    const qreal scale = interpolate(params.scale.from, params.scale.to, t);

    data.setXScale(scale);
    data.setYScale(scale);
    data.setXTranslation(0.5 * (1.0 - scale) * w->width());
    data.setYTranslation(0.5 * (1.0 - scale) * w->height());
    data.multiplyOpacity(interpolate(params.opacity.from, params.opacity.to, t));

    effects->paintWindow(w, mask, region, data);
}

void ScaleEffect::postPaintScreen()
{
    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        EffectWindow *w = animationIt.key();

        const QRect geo = w->expandedGeometry();
        const ScaleParams params = w->isDeleted() ? m_outParams : m_inParams;
        const qreal scale = qMax(params.scale.from, params.scale.to);
        const QRect repaintRect(
            geo.topLeft() + 0.5 * (1.0 - scale) * QPoint(geo.width(), geo.height()),
            geo.size() * scale);
        effects->addRepaint(repaintRect);

        if ((*animationIt).done()) {
            if (w->isDeleted()) {
                w->unrefWindow();
            } else {
                w->setData(WindowForceBackgroundContrastRole, QVariant());
                w->setData(WindowForceBlurRole, QVariant());
            }
            animationIt = m_animations.erase(animationIt);
        } else {
            ++animationIt;
        }
    }

    effects->postPaintScreen();
}

bool ScaleEffect::isActive() const
{
    return !m_animations.isEmpty();
}

bool ScaleEffect::supported()
{
    return effects->animationsSupported();
}

void ScaleEffect::windowAdded(EffectWindow *w)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    if (!isScaleWindow(w)) {
        return;
    }

    if (!w->isVisible()) {
        return;
    }

    const void *addGrab = w->data(WindowAddedGrabRole).value<void*>();
    if (addGrab && addGrab != this) {
        return;
    }

    TimeLine &timeLine = m_animations[w];
    timeLine.reset();
    timeLine.setDirection(TimeLine::Forward);
    timeLine.setDuration(m_duration);
    timeLine.setEasingCurve(QEasingCurve::InCurve);

    w->setData(WindowAddedGrabRole, QVariant::fromValue(static_cast<void*>(this)));
    w->setData(WindowForceBackgroundContrastRole, QVariant(true));
    w->setData(WindowForceBlurRole, QVariant(true));

    w->addRepaintFull();
}

void ScaleEffect::windowClosed(EffectWindow *w)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    if (!isScaleWindow(w)) {
        return;
    }

    if (!w->isVisible()) {
        return;
    }

    const void *closeGrab = w->data(WindowClosedGrabRole).value<void*>();
    if (closeGrab && closeGrab != this) {
        return;
    }

    w->refWindow();

    TimeLine &timeLine = m_animations[w];
    timeLine.reset();
    timeLine.setDirection(TimeLine::Forward);
    timeLine.setDuration(m_duration);
    timeLine.setEasingCurve(QEasingCurve::OutCurve);

    w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void*>(this)));
    w->setData(WindowForceBackgroundContrastRole, QVariant(true));
    w->setData(WindowForceBlurRole, QVariant(true));

    w->addRepaintFull();
}

void ScaleEffect::windowDeleted(EffectWindow *w)
{
    m_animations.remove(w);
}

void ScaleEffect::windowDataChanged(EffectWindow *w, int role)
{
    if (role != WindowAddedGrabRole && role != WindowClosedGrabRole) {
        return;
    }

    if (w->data(role).value<void*>() == this) {
        return;
    }

    auto animationIt = m_animations.find(w);
    if (animationIt == m_animations.end()) {
        return;
    }

    if (w->isDeleted() && role == WindowClosedGrabRole) {
        w->unrefWindow();
    }

    m_animations.erase(animationIt);

    w->setData(WindowForceBackgroundContrastRole, QVariant());
    w->setData(WindowForceBlurRole, QVariant());
}

bool ScaleEffect::isScaleWindow(EffectWindow *w) const
{
    // We don't want to animate most of plasmashell's windows, yet, some
    // of them we want to, for example, Task Manager Settings window.
    // The problem is that all those window share single window class.
    // So, the only way to decide whether a window should be animated is
    // to use a heuristic: if a window has decoration, then it's most
    // likely a dialog or a settings window so we have to animate it.
    if (w->windowClass() == QLatin1String("plasmashell plasmashell")) {
        return w->hasDecoration();
    }

    if (s_blacklist.contains(w->windowClass())) {
        return false;
    }

    if (w->hasDecoration()) {
        return true;
    }

    if (!w->isManaged()) {
        return false;
    }

    return w->isNormalWindow()
        || w->isDialog();
}

} // namespace KWin
