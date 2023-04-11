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
#include <qmath.h>

#include <cmath>

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

static QMatrix4x4 createPerspectiveMatrix(const QRectF &rect, const qreal scale)
{
    QMatrix4x4 ret;

    const float fovY = std::tan(qDegreesToRadians(60.0f) / 2);
    const float aspect = 1.0f;
    const float zNear = 0.1f;
    const float zFar = 100.0f;

    const float yMax = zNear * fovY;
    const float yMin = -yMax;
    const float xMin = yMin * aspect;
    const float xMax = yMax * aspect;

    ret.frustum(xMin, xMax, yMin, yMax, zNear, zFar);

    const auto deviceRect = scaledRect(rect, scale);

    const float scaleFactor = 1.1 * fovY / yMax;
    ret.translate(xMin * scaleFactor, yMax * scaleFactor, -1.1);
    ret.scale((xMax - xMin) * scaleFactor / deviceRect.width(),
              -(yMax - yMin) * scaleFactor / deviceRect.height(),
              0.001);
    ret.translate(-deviceRect.x(), -deviceRect.y());

    return ret;
}

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

void GlideEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        (*animationIt).timeLine.advance(presentTime);
        ++animationIt;
    }

    data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, presentTime);
}

void GlideEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (m_animations.contains(w)) {
        data.setTransformed();
    }

    effects->prePaintWindow(w, data, presentTime);
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

    const QMatrix4x4 oldProjMatrix = createPerspectiveMatrix(effects->renderTargetRect(), effects->renderTargetScale());
    const auto frame = w->frameGeometry();
    const auto scale = effects->renderTargetScale();
    const QRectF windowGeo = scaledRect(frame, scale);
    const QVector3D invOffset = oldProjMatrix.map(QVector3D(windowGeo.center()));
    QMatrix4x4 invOffsetMatrix;
    invOffsetMatrix.translate(invOffset.x(), invOffset.y());

    data.setProjectionMatrix(invOffsetMatrix * oldProjMatrix);

    // Move the center of the window to the origin.
    const QPointF offset = effects->renderTargetRect().center() - w->frameGeometry().center();
    data.translate(offset.x(), offset.y());

    const GlideParams params = w->isDeleted() ? m_outParams : m_inParams;
    const qreal t = (*animationIt).timeLine.value();

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
        if ((*animationIt).timeLine.done()) {
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

    if (!w->isVisible() || w->skipsCloseAnimation()) {
        return;
    }

    const void *closeGrab = w->data(WindowClosedGrabRole).value<void *>();
    if (closeGrab && closeGrab != this) {
        return;
    }

    w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void *>(this)));

    GlideAnimation &animation = m_animations[w];
    animation.deletedRef = EffectWindowDeletedRef(w);
    animation.visibleRef = EffectWindowVisibleRef(w, EffectWindow::PAINT_DISABLED_BY_DELETE);
    animation.timeLine.reset();
    animation.timeLine.setDirection(TimeLine::Forward);
    animation.timeLine.setDuration(m_duration);
    animation.timeLine.setEasingCurve(QEasingCurve::OutCurve);

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

    if (w->data(role).value<void *>() == this) {
        return;
    }

    auto animationIt = m_animations.find(w);
    if (animationIt != m_animations.end()) {
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

} // namespace KWin
