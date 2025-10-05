/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Marco Martin notmart @gmail.com
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "slidingpopups.h"
#include "slidingpopupsconfig.h"

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
        s_slideManagerRemoveTimer->callOnTimeout([]() {
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
    m_animationsData.clear();
}

bool SlidingPopupsEffect::supported()
{
    return effects->animationsSupported();
}

void SlidingPopupsEffect::reconfigure(ReconfigureFlags flags)
{
    AnimationEffect::reconfigure(flags);

    SlidingPopupsConfig::self()->read();
    // Keep these durations in sync with the value of Kirigami.Units.longDuration
    m_slideInDuration = std::chrono::milliseconds(
        static_cast<int>(animationTime(SlidingPopupsConfig::slideInTime() != 0 ? std::chrono::milliseconds(SlidingPopupsConfig::slideInTime()) : 200ms)));
    m_slideOutDuration = std::chrono::milliseconds(
        static_cast<int>(animationTime(SlidingPopupsConfig::slideOutTime() != 0 ? std::chrono::milliseconds(SlidingPopupsConfig::slideOutTime()) : 200ms)));

    auto dataIt = m_animationsData.begin();
    while (dataIt != m_animationsData.end()) {
        (*dataIt).slideInDuration = m_slideInDuration;
        (*dataIt).slideOutDuration = m_slideOutDuration;
        ++dataIt;
    }
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
    const QRectF screenRect = effects->clientArea(FullScreenArea, w->screen(), effects->currentDesktop());
    const QRectF windowGeo = w->frameGeometry();

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

    // Dave, WTF is offset for?

    // why don't we do slideLenghth here too?

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

QPointF SlidingPopupsEffect::translation(const AnimationData &animData)
{
    // TODO this is more confusing. There's a length and an offset

    int slideLength = animData.slideLength;
    if (slideLength <= 0) {
        slideLength = m_slideLength;
    }
    switch (animData.location) {
    case Location::Top:
        return QPointF(0, -slideLength);
    case Location::Left:
        return QPointF(-slideLength, 0);
    case Location::Bottom:
        return QPointF(0, slideLength);
    case Location::Right:
        return QPointF(slideLength, 0);
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

    if (!w->isVisible()) {
        return;
    }

    auto dataIt = m_animationsData.constFind(w);
    if (dataIt == m_animationsData.constEnd()) {
        return;
    }

    const QPointF from = translation(*dataIt);

    if (m_animations.contains(w)) {
        retarget(m_animations[w], FPx2(QPointF()), dataIt->slideInDuration.count()); // DAVE duration. Should we just change direction?
    } else {
        //(EffectWindow *w, Attribute a, uint meta, int ms, const FPx2 &to, const QEasingCurve &curve = QEasingCurve(), int delay = 0, const FPx2 &from = FPx2(), bool fullScreen = false, bool keepAlive = true, GLShader *shader = nullptr)
        m_animations[w] = set(w, AnimationEffect::Translation, 0, dataIt->slideInDuration.count(), FPx2(QPointF()), QEasingCurve::OutCubic, 0, FPx2(from), false, false);
    }
    qDebug() << "in " << w;

    // TODO opacity

    w->setData(WindowAddedGrabRole, QVariant::fromValue(static_cast<void *>(this)));
    w->setData(WindowForceBackgroundContrastRole, QVariant(true));
    w->setData(WindowForceBlurRole, QVariant(true));

    w->addRepaintFull();
}

void SlidingPopupsEffect::slideOut(EffectWindow *w)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    if (!w->isVisible()) {
        return;
    }

    auto dataIt = m_animationsData.constFind(w);
    if (dataIt == m_animationsData.constEnd()) {
        return;
    }

    const QPointF to = translation(*dataIt);

    if (m_animations.contains(w)) {
        retarget(m_animations[w], FPx2(to), dataIt->slideOutDuration.count());
    } else {
        //(EffectWindow *w, Attribute a, uint meta, int ms, const FPx2 &to, const QEasingCurve &curve = QEasingCurve(), int delay = 0, const FPx2 &from = FPx2(), bool fullScreen = false, bool keepAlive = true, GLShader *shader = nullptr)
        m_animations[w] = set(w, AnimationEffect::Position, 0, dataIt->slideOutDuration.count(), FPx2(to), QEasingCurve::OutCubic, 0, FPx2(QPointF()), false, false);
    }

    qDebug() << "out " << w;

    w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void *>(this)));
    w->setData(WindowForceBackgroundContrastRole, QVariant(true));
    w->setData(WindowForceBlurRole, QVariant(true));

    w->addRepaintFull();
}

void SlidingPopupsEffect::stopAnimations()
{
    for (const quint64 animationId : std::as_const(m_animations)) {
        qDebug() << "complete";
        complete(animationId);
    }

    m_animations.clear();
}

bool SlidingPopupsEffect::blocksDirectScanout() const
{
    return false;
}

void SlidingPopupsEffect::animationEnded(EffectWindow *w, Attribute a, uint meta)
{
    Q_UNUSED(a);
    Q_UNUSED(meta);
    m_animations.remove(w);

    // WHY IS THIS NOT BEING CALLED?
    qDebug() << "done " << w;
}

} // namespace

#include "moc_slidingpopups.cpp"
