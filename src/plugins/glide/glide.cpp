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

#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "scene/windowitem.h"

// Qt
#include <QMatrix4x4>
#include <QSet>
#include <qmath.h>

#include <cmath>

using namespace std::chrono_literals;

namespace KWin
{

static const QSet<QString> s_blacklist{
    QStringLiteral("ksmserver ksmserver"),
    QStringLiteral("ksmserver-logout-greeter ksmserver-logout-greeter"),
    QStringLiteral("ksplashqml ksplashqml"),
    // Spectacle needs to be blacklisted in order to stay out of its own screenshots.
    QStringLiteral("spectacle spectacle"), // x11
    QStringLiteral("spectacle org.kde.spectacle"), // wayland
};

GlideEffect::GlideEffect()
{
    GlideConfig::instance(effects->config());
    reconfigure(ReconfigureAll);

    connect(effects, &EffectsHandler::windowAdded, this, &GlideEffect::windowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &GlideEffect::windowClosed);
    connect(effects, &EffectsHandler::windowDataChanged, this, &GlideEffect::windowDataChanged);
}

GlideEffect::~GlideEffect() = default;

void GlideEffect::reconfigure(ReconfigureFlags flags)
{
    GlideConfig::self()->read();
    m_duration = std::chrono::milliseconds(animationTime<GlideConfig>(160ms));

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

void GlideEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    auto animationIt = m_animations.find(w);
    if (animationIt != m_animations.end()) {
        animationIt->second.timeLine.advance(presentTime);
        data.setTransformed();
    }

    effects->prePaintWindow(w, data, presentTime);
}

void GlideEffect::apply(EffectWindow *window, int mask, WindowPaintData &data, WindowQuadList &quads)
{
    auto animationIt = m_animations.find(window);
    if (animationIt == m_animations.end()) {
        return;
    }

    const GlideParams params = window->isDeleted() ? m_outParams : m_inParams;
    const qreal t = animationIt->second.timeLine.value();

    const QRectF rect = window->expandedGeometry().translated(-window->pos());
    const float fovY = std::tan(qDegreesToRadians(60.0f) / 2);
    const float aspect = rect.width() / rect.height();
    const float zNear = 0.1f;
    const float zFar = 100.0f;

    const float yMax = zNear * fovY;
    const float yMin = -yMax;
    const float xMin = yMin * aspect;
    const float xMax = yMax * aspect;

    const float scaleFactor = 1.1 * fovY / yMax;

    QMatrix4x4 matrix;
    matrix.viewport(rect);
    matrix.frustum(xMin, xMax, yMax, yMin, zNear, zFar);
    matrix.translate(xMin * scaleFactor, yMax * scaleFactor, -1.1);
    matrix.scale((xMax - xMin) * scaleFactor / rect.width(), -(yMax - yMin) * scaleFactor / rect.height(), 0.001);
    matrix.translate(-rect.x(), -rect.y());

    const qreal angle = interpolate(params.angle.from, params.angle.to, t);
    const qreal distance = interpolate(params.distance.from, params.distance.to, t);
    switch (params.edge) {
    case RotationEdge::Right:
        matrix.translate(window->width(), window->height() / 2, -distance);
        matrix.rotate(-angle, 0, 1, 0);
        matrix.translate(-window->width(), -window->height() / 2);
        break;

    case RotationEdge::Bottom:
        matrix.translate(window->width() / 2, window->height(), -distance);
        matrix.rotate(angle, 1, 0, 0);
        matrix.translate(-window->width() / 2, -window->height());
        break;

    case RotationEdge::Left:
        matrix.translate(0, window->height() / 2, -distance);
        matrix.rotate(angle, 0, 1, 0);
        matrix.translate(0, -window->height() / 2);
        break;

    case RotationEdge::Top:
    default:
        matrix.translate(window->width() / 2, 0, -distance);
        matrix.rotate(-angle, 1, 0, 0);
        matrix.translate(-window->width() / 2, 0);
        break;
    }

    quads = quads.makeRegularGrid(20, 20);
    for (WindowQuad &quad : quads) {
        for (int i = 0; i < 4; ++i) {
            const QPointF transformed = matrix.map(QPointF(quad[i].x(), quad[i].y()));
            quad[i].setX(transformed.x());
            quad[i].setY(transformed.y());
        }
    }

    data.multiplyOpacity(interpolate(params.opacity.from, params.opacity.to, t));
}

void GlideEffect::postPaintWindow(EffectWindow *w)
{
    if (auto animationIt = m_animations.find(w); animationIt != m_animations.end()) {
        w->addRepaintFull();

        if (animationIt->second.timeLine.done()) {
            unredirect(animationIt->first);
            animationIt = m_animations.erase(animationIt);
        } else {
            ++animationIt;
        }
    }

    effects->postPaintWindow(w);
}

bool GlideEffect::isActive() const
{
    return !m_animations.empty();
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

    const void *addGrab = w->data(WindowAddedGrabRole).value<void *>();
    if (addGrab && addGrab != this) {
        return;
    }

    w->setData(WindowAddedGrabRole, QVariant::fromValue(static_cast<void *>(this)));

    GlideAnimation &animation = m_animations[w];
    animation.timeLine.reset();
    animation.timeLine.setDirection(TimeLine::Forward);
    animation.timeLine.setDuration(m_duration);
    animation.timeLine.setEasingCurve(QEasingCurve::InCurve);
    animation.effect = ItemEffect(w->windowItem());

    redirect(w);
    effects->addRepaintFull();
}

void GlideEffect::windowClosed(EffectWindow *w)
{
    const void *closeGrab = w->data(WindowClosedGrabRole).value<void *>();
    if (closeGrab && closeGrab != this) {
        return;
    }
    if (effects->activeFullScreenEffect() || !isGlideWindow(w) || !w->isVisible() || w->skipsCloseAnimation()) {
        const auto it = m_animations.find(w);
        if (it != m_animations.end()) {
            unredirect(w);
            m_animations.erase(it);
        }
        return;
    }

    w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void *>(this)));

    GlideAnimation &animation = m_animations[w];
    animation.deletedRef = EffectWindowDeletedRef(w);
    animation.timeLine.reset();
    animation.timeLine.setDirection(TimeLine::Forward);
    animation.timeLine.setDuration(m_duration);
    animation.timeLine.setEasingCurve(QEasingCurve::OutCurve);

    redirect(w);
    effects->addRepaintFull();
}

void GlideEffect::windowDataChanged(EffectWindow *w, int role)
{
    if (role != WindowAddedGrabRole && role != WindowClosedGrabRole) {
        return;
    }

    if (w->data(role).value<void *>() == this) {
        return;
    }

    auto animationIt = m_animations.find(w);
    if (animationIt != m_animations.end()) {
        unredirect(animationIt->first);
        m_animations.erase(animationIt);
    }
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

    // Don't animate the outline and the screenlocker as it looks bad.
    if (w->isLockScreen() || w->isOutline()) {
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

bool GlideEffect::blocksDirectScanout() const
{
    return false;
}

} // namespace KWin

#include "moc_glide.cpp"
