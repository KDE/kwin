/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Marco Martin notmart @gmail.com
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "slidingpopups.h"
#include "slidingpopupsconfig.h"

#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "scene/windowitem.h"
#include "wayland/display.h"
#include "wayland/plasmashell.h"
#include "wayland/slide.h"
#include "wayland/surface.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QTimer>
#include <QWindow>

#include <algorithm>
#include <cmath>

#include <KWindowEffects>

Q_DECLARE_METATYPE(KWindowEffects::SlideFromLocation)

using namespace std::chrono_literals;

namespace KWin
{

SlideManagerInterface *SlidingPopupsEffect::s_slideManager = nullptr;
QTimer *SlidingPopupsEffect::s_slideManagerRemoveTimer = nullptr;

SlidingPopupsEffect::SlidingPopupsEffect()
{
    SlidingPopupsConfig::instance(effects->config());

    if (!s_slideManagerRemoveTimer) {
        s_slideManagerRemoveTimer = new QTimer(QCoreApplication::instance());
        s_slideManagerRemoveTimer->setSingleShot(true);
        s_slideManagerRemoveTimer->callOnTimeout(QCoreApplication::instance(), []() {
            s_slideManager->remove();
            s_slideManager = nullptr;
        });
    }
    s_slideManagerRemoveTimer->stop();
    if (!s_slideManager) {
        s_slideManager = new SlideManagerInterface(effects->waylandDisplay(), s_slideManagerRemoveTimer);
    }

    m_slideLength = QFontMetrics(QGuiApplication::font()).height() * 8;

    connect(effects, &EffectsHandler::windowAdded, this, &SlidingPopupsEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &SlidingPopupsEffect::slotWindowClosed);
    connect(effects, &EffectsHandler::windowDeleted, this, &SlidingPopupsEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::desktopChanged,
            this, &SlidingPopupsEffect::stopAnimations);
    connect(effects, &EffectsHandler::activeFullScreenEffectChanged,
            this, &SlidingPopupsEffect::stopAnimations);
    connect(effects, &EffectsHandler::screenLockingChanged,
            this, &SlidingPopupsEffect::stopAnimations);

    reconfigure(ReconfigureAll);

    const QList<EffectWindow *> windows = effects->stackingOrder();
    for (EffectWindow *window : windows) {
        setupSlideData(window);
    }
}

SlidingPopupsEffect::~SlidingPopupsEffect()
{
    // When compositing is restarted, avoid removing the manager immediately.
    if (s_slideManager) {
        s_slideManagerRemoveTimer->start(1000);
    }

    // Cancel animations here while both m_animations and m_animationsData are still valid.
    // slotWindowDeleted may access m_animationsData when an animation is removed.
    m_animations.clear();
    m_animationsData.clear();
}

bool SlidingPopupsEffect::supported()
{
    return effects->animationsSupported();
}

void SlidingPopupsEffect::reconfigure(ReconfigureFlags flags)
{
    SlidingPopupsConfig::self()->read();
    // Keep these durations in sync with the value of Kirigami.Units.longDuration
    m_slideInDuration = animationTime(SlidingPopupsConfig::slideInTime() != 0 ? std::chrono::milliseconds(SlidingPopupsConfig::slideInTime()) : 200ms);
    m_slideOutDuration = animationTime(SlidingPopupsConfig::slideOutTime() != 0 ? std::chrono::milliseconds(SlidingPopupsConfig::slideOutTime()) : 200ms);
    m_morphEasing = QEasingCurve(QEasingCurve::BezierSpline);
    m_morphEasing.addCubicBezierSegment(QPointF(0.2, 0.8), QPointF(0.2, 1.0), QPointF(1.0, 1.0));

    for (auto &[window, animation] : m_animations) {
        animation.timeLine.setDuration(animation.kind == AnimationKind::In ? m_slideInDuration : m_slideOutDuration);
    }

    auto dataIt = m_animationsData.begin();
    while (dataIt != m_animationsData.end()) {
        (*dataIt).slideInDuration = m_slideInDuration;
        (*dataIt).slideOutDuration = m_slideOutDuration;
        ++dataIt;
    }
}

void SlidingPopupsEffect::prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data)
{
    auto animationIt = m_animations.find(w);
    if (animationIt == m_animations.end()) {
        effects->prePaintWindow(view, w, data);
        return;
    }

    animationIt->second.timeLine.advance(view);
    data.setTransformed();

    effects->prePaintWindow(view, w, data);
}

bool SlidingPopupsEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceGeometry, WindowPaintData &data)
{
    auto animationIt = m_animations.find(w);
    if (animationIt == m_animations.end()) {
        return effects->paintWindow(renderTarget, viewport, w, mask, deviceGeometry, data);
    }

    if (animationIt->second.morph) {
        Region effectiveRegion = deviceGeometry;
        paintMorph(animationIt->second, w, data, effectiveRegion, viewport);
        return effects->paintWindow(renderTarget, viewport, w, mask, effectiveRegion, data);
    }

    const AnimationData &animData = m_animationsData[w];
    const qreal slideLength = (animData.slideLength > 0) ? animData.slideLength : m_slideLength;

    const RectF geo = w->expandedGeometry();
    const qreal t = animationIt->second.timeLine.value();

    Region effectiveRegion = deviceGeometry;
    switch (animData.location) {
    case Location::Left:
        if (slideLength < geo.width()) {
            data.multiplyOpacity(t);
        }
        data.translate(-interpolate(std::min(geo.width(), slideLength), 0.0, t));
        break;
    case Location::Top:
        if (slideLength < geo.height()) {
            data.multiplyOpacity(t);
        }
        data.translate(0.0, -interpolate(std::min(geo.height(), slideLength), 0.0, t));
        break;
    case Location::Right:
        if (slideLength < geo.width()) {
            data.multiplyOpacity(t);
        }
        data.translate(interpolate(std::min(geo.width(), slideLength), 0.0, t));
        break;
    case Location::Bottom:
    default:
        if (slideLength < geo.height()) {
            data.multiplyOpacity(t);
        }
        data.translate(0.0, interpolate(std::min(geo.height(), slideLength), 0.0, t));
    }

    effectiveRegion &= viewport.mapToDeviceCoordinatesAligned(damagedLogicalArea(w, animData));

    return effects->paintWindow(renderTarget, viewport, w, mask, effectiveRegion, data);
}

void SlidingPopupsEffect::postPaintScreen()
{
    for (auto animationIt = m_animations.begin(); animationIt != m_animations.end();) {
        EffectWindow *w = animationIt->first;
        if (animationIt->second.morph) {
            effects->addRepaint(w->expandedGeometry());
        } else {
            effects->addRepaint(damagedLogicalArea(w, m_animationsData[w]));
        }

        if (animationIt->second.timeLine.done()) {
            if (!w->isDeleted()) {
                w->setData(WindowForceBlurRole, QVariant());
            }
            animationIt = m_animations.erase(animationIt);
        } else {
            ++animationIt;
        }
    }

    effects->postPaintScreen();
}

void SlidingPopupsEffect::setupSlideData(EffectWindow *w)
{
    connect(w, &EffectWindow::windowHiddenChanged, this, &SlidingPopupsEffect::slotWindowHiddenChanged);

    if (effects->inputPanel() == w) {
        setupInputPanelSlide();
    } else if (auto surf = w->surface()) {
        slotWaylandSlideOnShowChanged(w);
        connect(surf, &SurfaceInterface::slideOnShowHideChanged, this, [this, surf] {
            slotWaylandSlideOnShowChanged(effects->findWindow(surf));
        });
    }

    if (auto internal = w->internalWindow()) {
        internal->installEventFilter(this);
        setupInternalWindowSlide(w);
    }
}

void SlidingPopupsEffect::slotWindowAdded(EffectWindow *w)
{
    setupSlideData(w);
    if (!w->isHidden()) {
        slideIn(w);
    }
}

void SlidingPopupsEffect::slotWindowClosed(EffectWindow *w)
{
    if (!w->isHidden()) {
        slideOut(w);
    }
}

void SlidingPopupsEffect::slotWindowDeleted(EffectWindow *w)
{
    m_animationsData.remove(w);
    m_morphs.remove(w);
}

void SlidingPopupsEffect::slotWindowHiddenChanged(EffectWindow *w)
{
    if (w->isHidden()) {
        slideOut(w);
    } else {
        slideIn(w);
    }
}

void SlidingPopupsEffect::setupAnimData(EffectWindow *w)
{
    const RectF screenRect = effects->clientArea(FullScreenArea, w->screen());
    const RectF windowGeo = w->frameGeometry();
    AnimationData &animData = m_animationsData[w];

    if (animData.offset == -1) {
        switch (animData.location) {
        case Location::Left:
            animData.offset = std::max<qreal>(windowGeo.left() - screenRect.left(), 0);
            break;
        case Location::Top:
            animData.offset = std::max<qreal>(windowGeo.top() - screenRect.top(), 0);
            break;
        case Location::Right:
            animData.offset = std::max<qreal>(screenRect.right() - windowGeo.right(), 0);
            break;
        case Location::Bottom:
        default:
            animData.offset = std::max<qreal>(screenRect.bottom() - windowGeo.bottom(), 0);
            break;
        }
    }
    // sanitize
    switch (animData.location) {
    case Location::Left:
        animData.offset = std::max<qreal>(0, animData.offset);
        break;
    case Location::Top:
        animData.offset = std::max<qreal>(0, animData.offset);
        break;
    case Location::Right:
        animData.offset = std::max<qreal>(0, animData.offset);
        break;
    case Location::Bottom:
    default:
        animData.offset = std::max<qreal>(0, animData.offset);
        break;
    }

    animData.slideInDuration = (animData.slideInDuration.count() != 0)
        ? animData.slideInDuration
        : m_slideInDuration;

    animData.slideOutDuration = (animData.slideOutDuration.count() != 0)
        ? animData.slideOutDuration
        : m_slideOutDuration;

    // Grab the window, so other windowClosed effects will ignore it
    w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void *>(this)));
}

void SlidingPopupsEffect::slotWaylandSlideOnShowChanged(EffectWindow *w)
{
    if (!w) {
        return;
    }

    SurfaceInterface *surf = w->surface();
    if (!surf) {
        return;
    }

    if (surf->slideOnShowHide()) {
        AnimationData &animData = m_animationsData[w];

        animData.offset = surf->slideOnShowHide()->offset();

        switch (surf->slideOnShowHide()->location()) {
        case SlideInterface::Location::Top:
            animData.location = Location::Top;
            break;
        case SlideInterface::Location::Left:
            animData.location = Location::Left;
            break;
        case SlideInterface::Location::Right:
            animData.location = Location::Right;
            break;
        case SlideInterface::Location::Bottom:
        default:
            animData.location = Location::Bottom;
            break;
        }
        animData.slideLength = 0;
        animData.slideInDuration = m_slideInDuration;
        animData.slideOutDuration = m_slideOutDuration;

        setupAnimData(w);
    }
}

void SlidingPopupsEffect::setupInternalWindowSlide(EffectWindow *w)
{
    if (!w) {
        return;
    }
    auto internal = w->internalWindow();
    if (!internal) {
        return;
    }
    const QVariant slideProperty = internal->property("kwin_slide");
    if (!slideProperty.isValid()) {
        return;
    }
    Location location;
    switch (slideProperty.value<KWindowEffects::SlideFromLocation>()) {
    case KWindowEffects::BottomEdge:
        location = Location::Bottom;
        break;
    case KWindowEffects::TopEdge:
        location = Location::Top;
        break;
    case KWindowEffects::RightEdge:
        location = Location::Right;
        break;
    case KWindowEffects::LeftEdge:
        location = Location::Left;
        break;
    default:
        return;
    }
    AnimationData &animData = m_animationsData[w];
    animData.location = location;
    bool intOk = false;
    animData.offset = internal->property("kwin_slide_offset").toInt(&intOk);
    if (!intOk) {
        animData.offset = -1;
    }
    animData.slideLength = 0;
    animData.slideInDuration = m_slideInDuration;
    animData.slideOutDuration = m_slideOutDuration;

    setupAnimData(w);
}

void SlidingPopupsEffect::setupInputPanelSlide()
{
    auto w = effects->inputPanel();

    if (!w || effects->isInputPanelOverlay()) {
        return;
    }

    AnimationData &animData = m_animationsData[w];
    animData.location = Location::Bottom;
    animData.offset = 0;
    animData.slideLength = 0;
    animData.slideInDuration = m_slideInDuration;
    animData.slideOutDuration = m_slideOutDuration;

    setupAnimData(w);
}

RectF SlidingPopupsEffect::damagedLogicalArea(EffectWindow *w, const AnimationData animData)
{
    const RectF screenRect = effects->clientArea(FullScreenArea, w->screen());
    const RectF geo = w->expandedGeometry();

    switch (animData.location) {
    case Location::Left:
        return RectF(QPointF(screenRect.left() + animData.offset, geo.top()),
                     QPointF(geo.right(), geo.bottom()));
    case Location::Top:
        return RectF(QPointF(geo.left(), screenRect.top() + animData.offset),
                     QPointF(geo.right(), geo.bottom()));
    case Location::Right:
        return RectF(QPointF(geo.left(), geo.top()),
                     QPointF(screenRect.right() - animData.offset, geo.bottom()));
    case Location::Bottom:
    default:
        return RectF(QPointF(geo.left(), geo.top()),
                     QPointF(geo.right(), screenRect.bottom() - animData.offset));
    }
}

bool SlidingPopupsEffect::eventFilter(QObject *watched, QEvent *event)
{
    auto internal = qobject_cast<QWindow *>(watched);
    if (internal && event->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *pe = static_cast<QDynamicPropertyChangeEvent *>(event);
        if (pe->propertyName() == "kwin_slide" || pe->propertyName() == "kwin_slide_offset") {
            if (auto w = effects->findWindow(internal)) {
                setupInternalWindowSlide(w);
            }
        }
    }
    return false;
}

void SlidingPopupsEffect::slideIn(EffectWindow *w)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    if (!w->isOnCurrentDesktop() || !w->isOnCurrentActivity()) {
        return;
    }

    auto dataIt = m_animationsData.constFind(w);
    if (dataIt == m_animationsData.constEnd()) {
        return;
    }

    Animation &animation = m_animations[w];
    animation.kind = AnimationKind::In;
    animation.morph = morphFor(w, (*dataIt).location);
    if (animation.morph) {
        m_morphs[w] = *animation.morph;
        // the way back is not the way there, so a morph always starts from the beginning
        animation.timeLine.reset();
        animation.timeLine.setDirection(TimeLine::Forward);
        animation.timeLine.setEasingCurve(QEasingCurve::Linear);
    } else {
        animation.timeLine.setDirection(TimeLine::Forward);
        animation.timeLine.setEasingCurve(QEasingCurve::OutCubic);
    }
    animation.timeLine.setDuration((*dataIt).slideInDuration);
    animation.windowEffect = ItemEffect(w->windowItem());

    // If the opposite animation (Out) was active and it had shorter duration,
    // at this point, the timeline can end up in the "done" state. Thus, we have
    // to reset it.
    if (animation.timeLine.done()) {
        animation.timeLine.reset();
    }

    w->setData(WindowAddedGrabRole, QVariant::fromValue(static_cast<void *>(this)));
    w->setData(WindowForceBlurRole, QVariant(true));

    w->addRepaintFull();
}

void SlidingPopupsEffect::slideOut(EffectWindow *w)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    if (!w->isOnCurrentDesktop() || !w->isOnCurrentActivity()) {
        return;
    }

    auto dataIt = m_animationsData.constFind(w);
    if (dataIt == m_animationsData.constEnd()) {
        return;
    }

    Animation &animation = m_animations[w];
    animation.deletedRef = EffectWindowDeletedRef(w);
    animation.visibleRef = EffectWindowVisibleRef(w, EffectWindow::PAINT_DISABLED);
    animation.kind = AnimationKind::Out;
    animation.morph = morphFor(w, (*dataIt).location);
    if (!animation.morph && m_morphs.contains(w)) {
        animation.morph = m_morphs.value(w);
    }
    if (animation.morph) {
        animation.timeLine.reset();
        animation.timeLine.setDirection(TimeLine::Forward);
        animation.timeLine.setEasingCurve(QEasingCurve::Linear);
    } else {
        animation.timeLine.setDirection(TimeLine::Backward);
        // this is effectively InCubic because the direction is reversed
        animation.timeLine.setEasingCurve(QEasingCurve::OutCubic);
    }
    animation.timeLine.setDuration((*dataIt).slideOutDuration);
    animation.windowEffect = ItemEffect(w->windowItem());

    // If the opposite animation (In) was active and it had shorter duration,
    // at this point, the timeline can end up in the "done" state. Thus, we have
    // to reset it.
    if (animation.timeLine.done()) {
        animation.timeLine.reset();
    }

    w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void *>(this)));
    w->setData(WindowForceBlurRole, QVariant(true));

    w->addRepaintFull();
}

std::optional<SlidingPopupsEffect::Morph> SlidingPopupsEffect::morphFor(EffectWindow *w, Location location) const
{
    SurfaceInterface *surface = w->surface();
    PlasmaShellSurfaceInterface *shellSurface = surface ? PlasmaShellSurfaceInterface::get(surface) : nullptr;
    if (!shellSurface || !shellSurface->popupOrigin()) {
        return std::nullopt;
    }
    const RectF frame = w->frameGeometry();
    if (frame.isEmpty()) {
        return std::nullopt;
    }

    Morph morph;
    morph.origin = RectF(*shellSurface->popupOrigin());
    const QPointF center = morph.origin.center();

    morph.leading = location == Location::Top || location == Location::Bottom ? Qt::Vertical : Qt::Horizontal;

    // The start is a small copy of the popup as wide (or tall) as what opens it, on the popup's side next to it.
    const qreal aspect = frame.width() / frame.height();
    if (morph.leading == Qt::Vertical) {
        const qreal width = std::min(morph.origin.width(), frame.width());
        const qreal height = width / aspect;
        const qreal x = std::clamp(center.x() - width / 2, frame.left(), frame.left() + frame.width() - width);
        const qreal y = location == Location::Bottom ? frame.top() + frame.height() - height : frame.top();
        morph.from = RectF(x, y, width, height);
    } else {
        const qreal height = std::min(morph.origin.height(), frame.height());
        const qreal width = height * aspect;
        const qreal y = std::clamp(center.y() - height / 2, frame.top(), frame.top() + frame.height() - height);
        const qreal x = location == Location::Right ? frame.left() + frame.width() - width : frame.left();
        morph.from = RectF(x, y, width, height);
    }
    return morph;
}

RectF SlidingPopupsEffect::morphBox(const Animation &animation, const RectF &frame) const
{
    const qreal t = animation.timeLine.value();
    const auto ease = [this](qreal x) {
        return m_morphEasing.valueForProgress(std::clamp(x, 0.0, 1.0));
    };
    // Each edge goes straight to its place. Opening, the leading axis runs a little ahead, so the popup
    // rises out of the panel before it spreads; closing mirrors it.
    const qreal fast = ease(t / 0.78);
    const qreal slow = ease((t - 0.07) / 0.93);
    const bool opening = animation.kind == AnimationKind::In;
    const qreal leadingProgress = opening ? fast : slow;
    const qreal otherProgress = opening ? slow : fast;
    const qreal ev = animation.morph->leading == Qt::Vertical ? leadingProgress : otherProgress;
    const qreal eh = animation.morph->leading == Qt::Vertical ? otherProgress : leadingProgress;

    const RectF &a = opening ? animation.morph->from : frame;
    const RectF &b = opening ? frame : animation.morph->from;
    const qreal left = interpolate(a.left(), b.left(), eh);
    const qreal right = interpolate(a.left() + a.width(), b.left() + b.width(), eh);
    const qreal top = interpolate(a.top(), b.top(), ev);
    const qreal bottom = interpolate(a.top() + a.height(), b.top() + b.height(), ev);
    return RectF(left, top, right - left, bottom - top);
}

void SlidingPopupsEffect::paintMorph(const Animation &animation, EffectWindow *w, WindowPaintData &data, Region &region, const RenderViewport &viewport) const
{
    const Morph &morph = *animation.morph;
    const RectF frame = w->frameGeometry();
    const RectF box = morphBox(animation, frame);

    const qreal scale = std::min(1.0, std::max(box.width() / frame.width(), box.height() / frame.height()));
    const qreal xScale = scale * std::sqrt(std::min(1.0, box.width() / (frame.width() * scale)));
    const qreal yScale = scale * std::sqrt(std::min(1.0, box.height() / (frame.height() * scale)));

    const qreal fx = std::clamp((morph.origin.center().x() - frame.left()) / frame.width(), 0.0, 1.0);
    const qreal fy = std::clamp((morph.origin.center().y() - frame.top()) / frame.height(), 0.0, 1.0);
    const RectF content(box.left() + (box.width() - frame.width() * xScale) * fx,
                        box.top() + (box.height() - frame.height() * yScale) * fy,
                        frame.width() * xScale,
                        frame.height() * yScale);

    data.setXScale(xScale);
    data.setYScale(yScale);
    data.translate(content.left() - frame.left(), content.top() - frame.top());

    const qreal e = m_morphEasing.valueForProgress(animation.timeLine.value());
    data.multiplyOpacity(animation.kind == AnimationKind::In ? e : 1.0 - e);

    // Clip to the box, except on sides where nothing is cropped, where the shadow may show past it.
    const RectF expanded = w->expandedGeometry();
    const qreal left = content.left() < box.left() - 0.5 ? box.left() : content.left() - (frame.left() - expanded.left()) * xScale;
    const qreal top = content.top() < box.top() - 0.5 ? box.top() : content.top() - (frame.top() - expanded.top()) * yScale;
    const qreal contentRight = content.left() + content.width();
    const qreal contentBottom = content.top() + content.height();
    const qreal right = contentRight > box.left() + box.width() + 0.5
        ? box.left() + box.width()
        : contentRight + ((expanded.left() + expanded.width()) - (frame.left() + frame.width())) * xScale;
    const qreal bottom = contentBottom > box.top() + box.height() + 0.5
        ? box.top() + box.height()
        : contentBottom + ((expanded.top() + expanded.height()) - (frame.top() + frame.height())) * yScale;
    region &= viewport.mapToDeviceCoordinatesAligned(RectF(left, top, right - left, bottom - top));
}

void SlidingPopupsEffect::stopAnimations()
{
    for (const auto &[window, animation] : m_animations) {
        if (!window->isDeleted()) {
            window->setData(WindowForceBlurRole, QVariant());
        }
    }

    m_animations.clear();
}

bool SlidingPopupsEffect::isActive() const
{
    return !m_animations.empty();
}

bool SlidingPopupsEffect::blocksDirectScanout() const
{
    return false;
}

} // namespace

#include "moc_slidingpopups.cpp"
