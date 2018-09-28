/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Marco Martin notmart@gmail.com

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

#include "slidingpopups.h"
#include "slidingpopupsconfig.h"

#include <QApplication>
#include <QFontMetrics>

#include <KWayland/Server/surface_interface.h>
#include <KWayland/Server/slide_interface.h>
#include <KWayland/Server/display.h>

namespace KWin
{

SlidingPopupsEffect::SlidingPopupsEffect()
{
    initConfig<SlidingPopupsConfig>();
    KWayland::Server::Display *display = effects->waylandDisplay();
    if (display) {
        display->createSlideManager(this)->create();
    }

    m_slideLength = QFontMetrics(qApp->font()).height() * 8;

    m_atom = effects->announceSupportProperty("_KDE_SLIDE", this);
    connect(effects, &EffectsHandler::windowAdded, this, &SlidingPopupsEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &SlidingPopupsEffect::slideOut);
    connect(effects, &EffectsHandler::windowDeleted, this, &SlidingPopupsEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::propertyNotify, this, &SlidingPopupsEffect::slotPropertyNotify);
    connect(effects, &EffectsHandler::windowShown, this, &SlidingPopupsEffect::slideIn);
    connect(effects, &EffectsHandler::windowHidden, this, &SlidingPopupsEffect::slideOut);
    connect(effects, &EffectsHandler::xcbConnectionChanged, this,
        [this] {
            m_atom = effects->announceSupportProperty(QByteArrayLiteral("_KDE_SLIDE"), this);
        }
    );
    reconfigure(ReconfigureAll);
}

SlidingPopupsEffect::~SlidingPopupsEffect()
{
}

bool SlidingPopupsEffect::supported()
{
     return effects->animationsSupported();
}

void SlidingPopupsEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)
    SlidingPopupsConfig::self()->read();
    m_slideInDuration = std::chrono::milliseconds(
        static_cast<int>(animationTime(SlidingPopupsConfig::slideInTime() != 0 ? SlidingPopupsConfig::slideInTime() : 150)));
    m_slideOutDuration = std::chrono::milliseconds(
        static_cast<int>(animationTime(SlidingPopupsConfig::slideOutTime() != 0 ? SlidingPopupsConfig::slideOutTime() : 250)));

    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        const auto duration = ((*animationIt).kind == AnimationKind::In)
            ? m_slideInDuration
            : m_slideOutDuration;
        (*animationIt).timeLine.setDuration(duration);
        ++animationIt;
    }

    auto dataIt = m_animationsData.begin();
    while (dataIt != m_animationsData.end()) {
        (*dataIt).slideInDuration = m_slideInDuration;
        (*dataIt).slideOutDuration = m_slideOutDuration;
        ++dataIt;
    }
}

void SlidingPopupsEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time)
{
    auto animationIt = m_animations.find(w);
    if (animationIt == m_animations.end()) {
        effects->prePaintWindow(w, data, time);
        return;
    }

    (*animationIt).timeLine.update(std::chrono::milliseconds(time));
    data.setTransformed();
    w->enablePainting(EffectWindow::PAINT_DISABLED | EffectWindow::PAINT_DISABLED_BY_DELETE);

    effects->prePaintWindow(w, data, time);
}

void SlidingPopupsEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    auto animationIt = m_animations.constFind(w);
    if (animationIt == m_animations.constEnd()) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    const AnimationData &animData = m_animationsData[w];
    const int slideLength = (animData.slideLength > 0) ? animData.slideLength : m_slideLength;

    const QRect screenRect = effects->clientArea(FullScreenArea, w->screen(), w->desktop());
    int splitPoint = 0;
    const QRect geo = w->expandedGeometry();
    const qreal t = (*animationIt).timeLine.value();

    switch (animData.location) {
    case Location::Left:
        if (slideLength < geo.width()) {
            data.multiplyOpacity(t);
        }
        data.translate(-interpolate(qMin(geo.width(), slideLength), 0.0, t));
        splitPoint = geo.width() - (geo.x() + geo.width() - screenRect.x() - animData.offset);
        region = QRegion(geo.x() + splitPoint, geo.y(), geo.width() - splitPoint, geo.height());
        break;
    case Location::Top:
        if (slideLength < geo.height()) {
            data.multiplyOpacity(t);
        }
        data.translate(0.0, -interpolate(qMin(geo.height(), slideLength), 0.0, t));
        splitPoint = geo.height() - (geo.y() + geo.height() - screenRect.y() - animData.offset);
        region = QRegion(geo.x(), geo.y() + splitPoint, geo.width(), geo.height() - splitPoint);
        break;
    case Location::Right:
        if (slideLength < geo.width()) {
            data.multiplyOpacity(t);
        }
        data.translate(interpolate(qMin(geo.width(), slideLength), 0.0, t));
        splitPoint = screenRect.x() + screenRect.width() - geo.x() - animData.offset;
        region = QRegion(geo.x(), geo.y(), splitPoint, geo.height());
        break;
    case Location::Bottom:
    default:
        if (slideLength < geo.height()) {
            data.multiplyOpacity(t);
        }
        data.translate(0.0, interpolate(qMin(geo.height(), slideLength), 0.0, t));
        splitPoint = screenRect.y() + screenRect.height() - geo.y() - animData.offset;
        region = QRegion(geo.x(), geo.y(), geo.width(), splitPoint);
    }

    effects->paintWindow(w, mask, region, data);
}

void SlidingPopupsEffect::postPaintWindow(EffectWindow *w)
{
    auto animationIt = m_animations.find(w);
    if (animationIt != m_animations.end()) {
        if ((*animationIt).timeLine.done()) {
            if (w->isDeleted()) {
                w->unrefWindow();
            } else {
                w->setData(WindowForceBackgroundContrastRole, QVariant());
                w->setData(WindowForceBlurRole, QVariant());
            }
            m_animations.erase(animationIt);
        }
        w->addRepaintFull();
    }

    effects->postPaintWindow(w);
}

void SlidingPopupsEffect::slotWindowAdded(EffectWindow *w)
{
    //X11
    if (m_atom != XCB_ATOM_NONE) {
        slotPropertyNotify(w, m_atom);
    }

    //Wayland
    if (auto surf = w->surface()) {
        slotWaylandSlideOnShowChanged(w);
        connect(surf, &KWayland::Server::SurfaceInterface::slideOnShowHideChanged, this, [this, surf] {
            slotWaylandSlideOnShowChanged(effects->findWindow(surf));
        });
    }

    slideIn(w);
}

void SlidingPopupsEffect::slotWindowDeleted(EffectWindow *w)
{
    m_animations.remove(w);
    m_animationsData.remove(w);
}

void SlidingPopupsEffect::slotPropertyNotify(EffectWindow *w, long atom)
{
    if (!w || atom != m_atom || m_atom == XCB_ATOM_NONE) {
        return;
    }

    // _KDE_SLIDE atom format(each field is an uint32_t):
    // <offset> <location> [<slide in duration>] [<slide out duration>] [<slide length>]
    //
    // If offset is equal to -1, this effect will decide what offset to use
    // given edge of the screen, from which the window has to slide.
    //
    // If slide in duration is equal to 0 milliseconds, the default slide in
    // duration will be used. Same with the slide out duration.
    //
    // NOTE: If only slide in duration has been provided, then it will be
    // also used as slide out duration. I.e. if you provided only slide in
    // duration, then slide in duration == slide out duration.

    const QByteArray rawAtomData = w->readProperty(m_atom, m_atom, 32);

    if (rawAtomData.isEmpty()) {
        // Property was removed, thus also remove the effect for window
        if (w->data(WindowClosedGrabRole).value<void *>() == this) {
            w->setData(WindowClosedGrabRole, QVariant());
        }
        m_animations.remove(w);
        m_animationsData.remove(w);
        return;
    }

    // Offset and location are required.
    if (static_cast<size_t>(rawAtomData.size()) < sizeof(uint32_t) * 2) {
        return;
    }

    const auto *atomData = reinterpret_cast<const uint32_t *>(rawAtomData.data());
    AnimationData &animData = m_animationsData[w];
    animData.offset = atomData[0];

    switch (atomData[1]) {
    case 0: // West
        animData.location = Location::Left;
        break;
    case 1: // North
        animData.location = Location::Top;
        break;
    case 2: // East
        animData.location = Location::Right;
        break;
    case 3: // South
    default:
        animData.location = Location::Bottom;
        break;
    }

    if (static_cast<size_t>(rawAtomData.size()) >= sizeof(uint32_t) * 3) {
        animData.slideInDuration = std::chrono::milliseconds(atomData[2]);
        if (static_cast<size_t>(rawAtomData.size()) >= sizeof(uint32_t) * 4) {
            animData.slideOutDuration = std::chrono::milliseconds(atomData[3]);
        } else {
            animData.slideOutDuration = animData.slideInDuration;
        }
    } else {
        animData.slideInDuration = m_slideInDuration;
        animData.slideOutDuration = m_slideOutDuration;
    }

    if (static_cast<size_t>(rawAtomData.size()) >= sizeof(uint32_t) * 5) {
        animData.slideLength = atomData[4];
    } else {
        animData.slideLength = 0;
    }

    setupAnimData(w);
}

void SlidingPopupsEffect::setupAnimData(EffectWindow *w)
{
    const QRect screenRect = effects->clientArea(FullScreenArea, w->screen(), effects->currentDesktop());
    const QRect windowGeo = w->geometry();
    AnimationData &animData = m_animationsData[w];

    if (animData.offset == -1) {
        switch (animData.location) {
        case Location::Left:
            animData.offset = qMax(windowGeo.left() - screenRect.left(), 0);
            break;
        case Location::Top:
            animData.offset = qMax(windowGeo.top() - screenRect.top(), 0);
            break;
        case Location::Right:
            animData.offset = qMax(screenRect.right() - windowGeo.right(), 0);
            break;
        case Location::Bottom:
        default:
            animData.offset = qMax(screenRect.bottom() - windowGeo.bottom(), 0);
            break;
        }
    }
    // sanitize
    switch (animData.location) {
    case Location::Left:
        animData.offset = qMax(windowGeo.left() - screenRect.left(), animData.offset);
        break;
    case Location::Top:
        animData.offset = qMax(windowGeo.top() - screenRect.top(), animData.offset);
        break;
    case Location::Right:
        animData.offset = qMax(screenRect.right() - windowGeo.right(), animData.offset);
        break;
    case Location::Bottom:
    default:
        animData.offset = qMax(screenRect.bottom() - windowGeo.bottom(), animData.offset);
        break;
    }

    animData.slideInDuration = (animData.slideInDuration.count() != 0)
        ? animData.slideInDuration
        : m_slideInDuration;

    animData.slideOutDuration = (animData.slideOutDuration.count() != 0)
        ? animData.slideOutDuration
        : m_slideOutDuration;

    // Grab the window, so other windowClosed effects will ignore it
    w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void*>(this)));
}

void SlidingPopupsEffect::slotWaylandSlideOnShowChanged(EffectWindow* w)
{
    if (!w) {
        return;
    }

    KWayland::Server::SurfaceInterface *surf = w->surface();
    if (!surf) {
        return;
    }

    if (surf->slideOnShowHide()) {
        AnimationData &animData = m_animationsData[w];

        animData.offset = surf->slideOnShowHide()->offset();

        switch (surf->slideOnShowHide()->location()) {
        case KWayland::Server::SlideInterface::Location::Top:
            animData.location = Location::Top;
            break;
        case KWayland::Server::SlideInterface::Location::Left:
            animData.location = Location::Left;
            break;
        case KWayland::Server::SlideInterface::Location::Right:
            animData.location = Location::Right;
            break;
        case KWayland::Server::SlideInterface::Location::Bottom:
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

    Animation &animation = m_animations[w];
    animation.kind = AnimationKind::In;
    animation.timeLine.reset();
    animation.timeLine.setDirection(TimeLine::Forward);
    animation.timeLine.setDuration((*dataIt).slideInDuration);
    animation.timeLine.setEasingCurve(QEasingCurve::InOutSine);

    w->setData(WindowAddedGrabRole, QVariant::fromValue(static_cast<void*>(this)));
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

    if (w->isDeleted()) {
        w->refWindow();
    }

    Animation &animation = m_animations[w];
    animation.kind = AnimationKind::Out;
    animation.timeLine.reset();
    animation.timeLine.setDirection(TimeLine::Backward);
    animation.timeLine.setDuration((*dataIt).slideOutDuration);
    animation.timeLine.setEasingCurve(QEasingCurve::InOutSine);

    w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void*>(this)));
    w->setData(WindowForceBackgroundContrastRole, QVariant(true));
    w->setData(WindowForceBlurRole, QVariant(true));

    w->addRepaintFull();
}

bool SlidingPopupsEffect::isActive() const
{
    return !m_animations.isEmpty();
}

} // namespace
