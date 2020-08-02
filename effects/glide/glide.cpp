/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Philip Falkner <philip.falkner@gmail.com>
    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2010 Alexandre Pereira <pereira.alex@gmail.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// own
#include "glide.h"

// KConfigSkeleton
#include "glideconfig.h"

// Qt
#include <QMatrix4x4>
#include <QSet>

namespace KWin
{

static const QSet<QString> s_blacklist {
    QStringLiteral("ksmserver ksmserver"),
    QStringLiteral("ksmserver-logout-greeter ksmserver-logout-greeter"),
    QStringLiteral("ksplashqml ksplashqml"),
    QStringLiteral("ksplashsimple ksplashsimple"),
    QStringLiteral("ksplashx ksplashx")
};

GlideEffect::GlideEffect()
{
    initConfig<GlideConfig>();
    reconfigure(ReconfigureAll);

    connect(effects, &EffectsHandler::windowAdded, this, &GlideEffect::windowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &GlideEffect::windowClosed);
    connect(effects, &EffectsHandler::windowDeleted, this, &GlideEffect::windowDeleted);
    connect(effects, &EffectsHandler::windowDataChanged, this, &GlideEffect::windowDataChanged);
}

GlideEffect::~GlideEffect() = default;

void GlideEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    GlideConfig::self()->read();
    m_duration = std::chrono::milliseconds(animationTime<GlideConfig>(160));

    m_inParams.edge = static_cast<RotationEdge>(GlideConfig::inRotationEdge());
    m_inParams.angle.from = GlideConfig::inRotationAngle();
    m_inParams.angle.to = 0.0;
    m_inParams.distance.from = GlideConfig::inDistance();
    m_inParams.distance.to = 0.0;
    m_inParams.opacity.from = GlideConfig::inOpacity();
    m_inParams.opacity.to = 1.0;

    m_outParams.edge = static_cast<RotationEdge>(GlideConfig::outRotationEdge());
    m_outParams.angle.from = 0.0;
    m_outParams.angle.to = GlideConfig::outRotationAngle();
    m_outParams.distance.from = 0.0;
    m_outParams.distance.to = GlideConfig::outDistance();
    m_outParams.opacity.from = 1.0;
    m_outParams.opacity.to = GlideConfig::outOpacity();
}

void GlideEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    const std::chrono::milliseconds delta(time);

    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        (*animationIt).update(delta);
        ++animationIt;
    }

    data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, time);
}

void GlideEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time)
{
    if (m_animations.contains(w)) {
        data.setTransformed();
        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DELETE);
    }

    effects->prePaintWindow(w, data, time);
}

void GlideEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    auto animationIt = m_animations.constFind(w);
    if (animationIt == m_animations.constEnd()) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    // Perspective projection distorts objects near edges
    // of the viewport. This is critical because distortions
    // near edges of the viewport are not desired with this effect.
    // To fix this, the center of the window will be moved to the origin,
    // after applying perspective projection, the center is moved back
    // to its "original" projected position. Overall, this is how the window
    // will be transformed:
    //  [move to the origin] -> [rotate] -> [translate] ->
    //    -> [perspective projection] -> [reverse "move to the origin"]
    const QMatrix4x4 oldProjMatrix = data.screenProjectionMatrix();
    const QRectF windowGeo = w->geometry();
    const QVector3D invOffset = oldProjMatrix.map(QVector3D(windowGeo.center()));
    QMatrix4x4 invOffsetMatrix;
    invOffsetMatrix.translate(invOffset.x(), invOffset.y());
    data.setProjectionMatrix(invOffsetMatrix * oldProjMatrix);

    // Move the center of the window to the origin.
    const QRectF screenGeo = effects->virtualScreenGeometry();
    const QPointF offset = screenGeo.center() - windowGeo.center();
    data.translate(offset.x(), offset.y());

    const GlideParams params = w->isDeleted() ? m_outParams : m_inParams;
    const qreal t = (*animationIt).value();

    switch (params.edge) {
    case RotationEdge::Top:
        data.setRotationAxis(Qt::XAxis);
        data.setRotationOrigin(QVector3D(0, 0, 0));
        data.setRotationAngle(-interpolate(params.angle.from, params.angle.to, t));
        break;

    case RotationEdge::Right:
        data.setRotationAxis(Qt::YAxis);
        data.setRotationOrigin(QVector3D(w->width(), 0, 0));
        data.setRotationAngle(-interpolate(params.angle.from, params.angle.to, t));
        break;

    case RotationEdge::Bottom:
        data.setRotationAxis(Qt::XAxis);
        data.setRotationOrigin(QVector3D(0, w->height(), 0));
        data.setRotationAngle(interpolate(params.angle.from, params.angle.to, t));
        break;

    case RotationEdge::Left:
        data.setRotationAxis(Qt::YAxis);
        data.setRotationOrigin(QVector3D(0, 0, 0));
        data.setRotationAngle(interpolate(params.angle.from, params.angle.to, t));
        break;

    default:
        // Fallback to Top.
        data.setRotationAxis(Qt::XAxis);
        data.setRotationOrigin(QVector3D(0, 0, 0));
        data.setRotationAngle(-interpolate(params.angle.from, params.angle.to, t));
        break;
    }

    data.setZTranslation(-interpolate(params.distance.from, params.distance.to, t));
    data.multiplyOpacity(interpolate(params.opacity.from, params.opacity.to, t));

    effects->paintWindow(w, mask, region, data);
}

void GlideEffect::postPaintScreen()
{
    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        if ((*animationIt).done()) {
            EffectWindow *w = animationIt.key();
            if (w->isDeleted()) {
                w->unrefWindow();
            }
            animationIt = m_animations.erase(animationIt);
        } else {
            ++animationIt;
        }
    }

    effects->addRepaintFull();
    effects->postPaintScreen();
}

bool GlideEffect::isActive() const
{
    return !m_animations.isEmpty();
}

bool GlideEffect::supported()
{
    return effects->isOpenGLCompositing()
        && effects->animationsSupported();
}

void GlideEffect::windowAdded(EffectWindow *w)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    if (!isGlideWindow(w)) {
        return;
    }

    if (!w->isVisible()) {
        return;
    }

    const void *addGrab = w->data(WindowAddedGrabRole).value<void*>();
    if (addGrab && addGrab != this) {
        return;
    }

    w->setData(WindowAddedGrabRole, QVariant::fromValue(static_cast<void*>(this)));

    TimeLine &timeLine = m_animations[w];
    timeLine.reset();
    timeLine.setDirection(TimeLine::Forward);
    timeLine.setDuration(m_duration);
    timeLine.setEasingCurve(QEasingCurve::InCurve);

    effects->addRepaintFull();
}

void GlideEffect::windowClosed(EffectWindow *w)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    if (!isGlideWindow(w)) {
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
    w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void*>(this)));

    TimeLine &timeLine = m_animations[w];
    timeLine.reset();
    timeLine.setDirection(TimeLine::Forward);
    timeLine.setDuration(m_duration);
    timeLine.setEasingCurve(QEasingCurve::OutCurve);

    effects->addRepaintFull();
}

void GlideEffect::windowDeleted(EffectWindow *w)
{
    m_animations.remove(w);
}

void GlideEffect::windowDataChanged(EffectWindow *w, int role)
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
}

bool GlideEffect::isGlideWindow(EffectWindow *w) const
{
    // We don't want to animate most of plasmashell's windows, yet, some
    // of them we want to, for example, Task Manager Settings window.
    // The problem is that all those window share single window class.
    // So, the only way to decide whether a window should be animated is
    // to use a heuristic: if a window has decoration, then it's most
    // likely a dialog or a settings window so we have to animate it.
    if (w->windowClass() == QLatin1String("plasmashell plasmashell")
            || w->windowClass() == QLatin1String("plasmashell org.kde.plasmashell")) {
        return w->hasDecoration();
    }

    if (s_blacklist.contains(w->windowClass())) {
        return false;
    }

    if (w->hasDecoration()) {
        return true;
    }

    // Don't animate combobox popups, tooltips, popup menus, etc.
    if (w->isPopupWindow()) {
        return false;
    }

    // Don't animate the outline because it looks very sick.
    if (w->isOutline()) {
        return false;
    }

    // Override-redirect windows are usually used for user interface
    // concepts that are not expected to be animated by this effect.
    if (w->isX11Client() && !w->isManaged()) {
        return false;
    }

    return w->isNormalWindow()
        || w->isDialog();
}

} // namespace KWin
