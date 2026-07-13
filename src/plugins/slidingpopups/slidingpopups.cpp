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
#include "wayland/slide.h"
#include "wayland/surface.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QTimer>
#include <QWindow>

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

    Animation &animation = animationIt->second;
    const AnimationData &animData = m_animationsData[w];

    // NOTE that expandedGeometry follows our translation
    // because it's calculated from the items
    const RectF geo{w->pos(), w->expandedGeometry().size()};
    const RectF screenRect = effects->clientArea(FullScreenArea, w->screen());

    animation.timeLine.advance(view);
    const qreal t = animation.timeLine.value();

    RectF clipRect = screenRect;
    switch (animData.location) {
    case Location::Left: {
        animation.transform->setOpacity(m_slideLength < geo.width() ? t : 1.0);
        animation.transform->setPosition(QPointF(-interpolate(std::min(geo.width(), m_slideLength), 0.0, t), 0));
        clipRect.setLeft(screenRect.left() + animData.offset);
    } break;
    case Location::Top: {
        animation.transform->setOpacity(m_slideLength < geo.height() ? t : 1.0);
        animation.transform->setPosition(QPointF(0.0, -interpolate(std::min(geo.height(), m_slideLength), 0.0, t)));
        clipRect.setTop(screenRect.top() + animData.offset);
    } break;
    case Location::Right: {
        animation.transform->setOpacity(m_slideLength < geo.width() ? t : 1.0);
        animation.transform->setPosition(QPointF(interpolate(std::min(geo.width(), m_slideLength), 0.0, t), 0));
        clipRect.setRight(screenRect.right() - animData.offset);
    } break;
    case Location::Bottom:
    default: {
        animation.transform->setOpacity(m_slideLength < geo.height() ? t : 1.0);
        animation.transform->setPosition(QPointF(0.0, interpolate(std::min(geo.height(), m_slideLength), 0.0, t)));
        clipRect.setBottom(screenRect.bottom() - animData.offset);
    }
    }
    animation.transform->setGlobalClipRect(clipRect);

    effects->prePaintWindow(view, w, data);
}

void SlidingPopupsEffect::postPaintScreen()
{
    for (auto animationIt = m_animations.begin(); animationIt != m_animations.end();) {
        auto &[window, animation] = *animationIt;

        if (animation.timeLine.done()) {
            animationIt = m_animations.erase(animationIt);
        } else {
            if (!animation.transform->scheduleFrameRecursive()) {
                // Because the animation can start with the window completely
                // off-screen, scheduling frames with the item can no-op
                // FIXME find a better solution for this
                effects->addRepaintFull();
            }
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
    animData.slideInDuration = m_slideInDuration;
    animData.slideOutDuration = m_slideOutDuration;

    setupAnimData(w);
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
    animation.timeLine.setDirection(TimeLine::Forward);
    animation.timeLine.setDuration((*dataIt).slideInDuration);
    animation.timeLine.setEasingCurve(QEasingCurve::OutCubic);
    animation.transform = std::make_unique<TransformItem>(w);
    animation.transform->scheduleFrameRecursive();

    // If the opposite animation (Out) was active and it had shorter duration,
    // at this point, the timeline can end up in the "done" state. Thus, we have
    // to reset it.
    if (animation.timeLine.done()) {
        animation.timeLine.reset();
    }

    w->setData(WindowAddedGrabRole, QVariant::fromValue(static_cast<void *>(this)));
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
    animation.timeLine.setDirection(TimeLine::Backward);
    animation.timeLine.setDuration((*dataIt).slideOutDuration);
    // this is effectively InCubic because the direction is reversed
    animation.timeLine.setEasingCurve(QEasingCurve::OutCubic);
    animation.transform = std::make_unique<TransformItem>(w);
    animation.transform->scheduleFrameRecursive();

    // If the opposite animation (In) was active and it had shorter duration,
    // at this point, the timeline can end up in the "done" state. Thus, we have
    // to reset it.
    if (animation.timeLine.done()) {
        animation.timeLine.reset();
    }

    w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void *>(this)));
}

void SlidingPopupsEffect::stopAnimations()
{
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
