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

    mSlideLength = QFontMetrics(qApp->font()).height() * 8;

    mAtom = effects->announceSupportProperty("_KDE_SLIDE", this);
    connect(effects, SIGNAL(windowAdded(KWin::EffectWindow*)), this, SLOT(slotWindowAdded(KWin::EffectWindow*)));
    connect(effects, &EffectsHandler::windowClosed, this, &SlidingPopupsEffect::slideOut);
    connect(effects, SIGNAL(windowDeleted(KWin::EffectWindow*)), this, SLOT(slotWindowDeleted(KWin::EffectWindow*)));
    connect(effects, SIGNAL(propertyNotify(KWin::EffectWindow*,long)), this, SLOT(slotPropertyNotify(KWin::EffectWindow*,long)));
    connect(effects, &EffectsHandler::windowShown, this, &SlidingPopupsEffect::slideIn);
    connect(effects, &EffectsHandler::windowHidden, this, &SlidingPopupsEffect::slideOut);
    connect(effects, &EffectsHandler::xcbConnectionChanged, this,
        [this] {
            mAtom = effects->announceSupportProperty(QByteArrayLiteral("_KDE_SLIDE"), this);
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
    mFadeInTime = std::chrono::milliseconds(
        static_cast<int>(animationTime(SlidingPopupsConfig::slideInTime() != 0 ? SlidingPopupsConfig::slideInTime() : 150)));
    mFadeOutTime = std::chrono::milliseconds(
        static_cast<int>(animationTime(SlidingPopupsConfig::slideOutTime() != 0 ? SlidingPopupsConfig::slideOutTime() : 250)));

    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        const auto duration = ((*animationIt).kind == AnimationKind::In)
            ? mFadeInTime
            : mFadeOutTime;
        (*animationIt).timeLine.setDuration(duration);
        ++animationIt;
    }

    QHash< const EffectWindow*, Data >::iterator wIt = mWindowsData.begin();
    while (wIt != mWindowsData.end()) {
        wIt.value().fadeInDuration = mFadeInTime;
        wIt.value().fadeOutDuration = mFadeOutTime;
        ++wIt;
    }
}

void SlidingPopupsEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    auto animationIt = m_animations.find(w);
    if (animationIt == m_animations.end()) {
        effects->prePaintWindow(w, data, time);
        return;
    }

    (*animationIt).timeLine.update(std::chrono::milliseconds(time));
    data.setTransformed();
    w->enablePainting(EffectWindow::PAINT_DISABLED | EffectWindow::PAINT_DISABLED_BY_DELETE);

    const int start = mWindowsData[ w ].start;
    if (start != 0) {
        const QRect screenRect = effects->clientArea(FullScreenArea, w->screen(), effects->currentDesktop());
        const QRect geo = w->expandedGeometry();
        const qreal t = (*animationIt).timeLine.value();
        // filter out window quads, but only if the window does not start from the edge
        int slideLength;
        if (mWindowsData[ w ].slideLength > 0) {
            slideLength = mWindowsData[ w ].slideLength;
        } else {
            slideLength = mSlideLength;
        }

        switch(mWindowsData[ w ].from) {
        case West: {
            const double splitPoint = geo.width() - (geo.x() + geo.width() - screenRect.x() - start) + interpolate(qMin(geo.width(), slideLength), 0, t);
            data.quads = data.quads.splitAtX(splitPoint);
            WindowQuadList filtered;
            filtered.reserve(data.quads.count());
            for (const WindowQuad &quad : qAsConst(data.quads)) {
                if (quad.left() >= splitPoint) {
                    filtered << quad;
                }
            }
            data.quads = filtered;
            break;
        }
        case North: {
            const double splitPoint = geo.height() - (geo.y() + geo.height() - screenRect.y() - start) + interpolate(qMin(geo.height(), slideLength), 0, t);
            data.quads = data.quads.splitAtY(splitPoint);
            WindowQuadList filtered;
            filtered.reserve(data.quads.count());
            for (const WindowQuad &quad : qAsConst(data.quads)) {
                if (quad.top() >= splitPoint) {
                    filtered << quad;
                }
            }
            data.quads = filtered;
            break;
        }
        case East: {
            const double splitPoint = screenRect.x() + screenRect.width() - geo.x() - start - interpolate(qMin(geo.width(), slideLength), 0, t);
            data.quads = data.quads.splitAtX(splitPoint);
            WindowQuadList filtered;
            filtered.reserve(data.quads.count());
            for (const WindowQuad &quad : qAsConst(data.quads)) {
                if (quad.right() <= splitPoint) {
                    filtered << quad;
                }
            }
            data.quads = filtered;
            break;
        }
        case South:
        default: {
            const double splitPoint = screenRect.y() + screenRect.height() - geo.y() - start - interpolate(qMin(geo.height(), slideLength), 0, t);
            data.quads = data.quads.splitAtY(splitPoint);
            WindowQuadList filtered;
            filtered.reserve(data.quads.count());
            for (const WindowQuad &quad : qAsConst(data.quads)) {
                if (quad.bottom() <= splitPoint) {
                    filtered << quad;
                }
            }
            data.quads = filtered;
            break;
        }
        }
    }

    effects->prePaintWindow(w, data, time);
}

void SlidingPopupsEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    auto animationIt = m_animations.constFind(w);
    if (animationIt == m_animations.constEnd()) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    const int start = mWindowsData[ w ].start;

    int slideLength;
    if (mWindowsData[ w ].slideLength > 0) {
        slideLength = mWindowsData[ w ].slideLength;
    } else {
        slideLength = mSlideLength;
    }

    const QRect screenRect = effects->clientArea(FullScreenArea, w->screen(), w->desktop());
    int splitPoint = 0;
    const QRect geo = w->expandedGeometry();
    const qreal t = (*animationIt).timeLine.value();

    switch(mWindowsData[ w ].from) {
    case West:
        if (slideLength < geo.width()) {
            data.multiplyOpacity(t);
        }
        data.translate(-interpolate(qMin(geo.width(), slideLength), 0.0, t));
        splitPoint = geo.width() - (geo.x() + geo.width() - screenRect.x() - start);
        region = QRegion(geo.x() + splitPoint, geo.y(), geo.width() - splitPoint, geo.height());
        break;
    case North:
        if (slideLength < geo.height()) {
            data.multiplyOpacity(t);
        }
        data.translate(0.0, -interpolate(qMin(geo.height(), slideLength), 0.0, t));
        splitPoint = geo.height() - (geo.y() + geo.height() - screenRect.y() - start);
        region = QRegion(geo.x(), geo.y() + splitPoint, geo.width(), geo.height() - splitPoint);
        break;
    case East:
        if (slideLength < geo.width()) {
            data.multiplyOpacity(t);
        }
        data.translate(interpolate(qMin(geo.width(), slideLength), 0.0, t));
        splitPoint = screenRect.x() + screenRect.width() - geo.x() - start;
        region = QRegion(geo.x(), geo.y(), splitPoint, geo.height());
        break;
    case South:
    default:
        if (slideLength < geo.height()) {
            data.multiplyOpacity(t);
        }
        data.translate(0.0, interpolate(qMin(geo.height(), slideLength), 0.0, t));
        splitPoint = screenRect.y() + screenRect.height() - geo.y() - start;
        region = QRegion(geo.x(), geo.y(), geo.width(), splitPoint);
    }

    effects->paintWindow(w, mask, region, data);
}

void SlidingPopupsEffect::postPaintWindow(EffectWindow* w)
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
    if (mAtom != XCB_ATOM_NONE) {
        slotPropertyNotify(w, mAtom);
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

void SlidingPopupsEffect::slotWindowDeleted(EffectWindow* w)
{
    m_animations.remove(w);
    mWindowsData.remove(w);
    effects->addRepaint(w->expandedGeometry());
}

void SlidingPopupsEffect::slotPropertyNotify(EffectWindow* w, long a)
{
    if (!w || a != mAtom || mAtom == XCB_ATOM_NONE)
        return;

    QByteArray data = w->readProperty(mAtom, mAtom, 32);

    if (data.length() < 1) {
        // Property was removed, thus also remove the effect for window
        if (w->data(WindowClosedGrabRole).value<void *>() == this) {
            w->setData(WindowClosedGrabRole, QVariant());
        }
        m_animations.remove(w);
        mWindowsData.remove(w);
        return;
    }

    auto* d = reinterpret_cast< uint32_t* >(data.data());
    Data animData;
    animData.start = d[ 0 ];
    animData.from = (Position)d[ 1 ];
    //custom duration
    animData.slideLength = 0;
    if (data.length() >= (int)(sizeof(uint32_t) * 3)) {
        animData.fadeInDuration = std::chrono::milliseconds(d[2]);
        if (data.length() >= (int)(sizeof(uint32_t) * 4))
            //custom fadein
            animData.fadeOutDuration = std::chrono::milliseconds(d[3]);
        else
            //custom fadeout
            animData.fadeOutDuration = std::chrono::milliseconds(d[2]);

        //do we want an actual slide?
        if (data.length() >= (int)(sizeof(uint32_t) * 5))
            animData.slideLength = d[4];
    } else {
        animData.fadeInDuration = mFadeInTime;
        animData.fadeOutDuration = mFadeOutTime;
    }
    mWindowsData[ w ] = animData;
    setupAnimData(w);
}

void SlidingPopupsEffect::setupAnimData(EffectWindow *w)
{
    const QRect screenRect = effects->clientArea(FullScreenArea, w->screen(), effects->currentDesktop());
    const QRect windowGeo = w->geometry();
    if (mWindowsData[w].start == -1) {
        switch (mWindowsData[w].from) {
        case West:
            mWindowsData[w].start = qMax(windowGeo.left() - screenRect.left(), 0);
            break;
        case North:
            mWindowsData[w].start = qMax(windowGeo.top() - screenRect.top(), 0);
            break;
        case East:
            mWindowsData[w].start = qMax(screenRect.right() - windowGeo.right(), 0);
            break;
        case South:
        default:
            mWindowsData[w].start = qMax(screenRect.bottom() - windowGeo.bottom(), 0);
            break;
        }
    }
    // sanitize
    int difference = 0;
    switch (mWindowsData[w].from) {
    case West:
        mWindowsData[w].start = qMax(windowGeo.left() - screenRect.left(), mWindowsData[w].start);
        break;
    case North:
        mWindowsData[w].start = qMax(windowGeo.top() - screenRect.top(), mWindowsData[w].start);
        break;
    case East:
        mWindowsData[w].start = qMax(screenRect.right() - windowGeo.right(), mWindowsData[w].start);
        break;
    case South:
    default:
        mWindowsData[w].start = qMax(screenRect.bottom() - windowGeo.bottom(), mWindowsData[w].start);
        break;
    }

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
        Data animData;
        animData.start = surf->slideOnShowHide()->offset();

        switch (surf->slideOnShowHide()->location()) {
        case KWayland::Server::SlideInterface::Location::Top:
            animData.from = North;
            break;
        case KWayland::Server::SlideInterface::Location::Left:
            animData.from = West;
            break;
        case KWayland::Server::SlideInterface::Location::Right:
            animData.from = East;
            break;
        case KWayland::Server::SlideInterface::Location::Bottom:
        default:
            animData.from = South;
            break;
        }
        animData.slideLength = 0;
        animData.fadeInDuration = mFadeInTime;
        animData.fadeOutDuration = mFadeOutTime;
        mWindowsData[ w ] = animData;

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

    auto dataIt = mWindowsData.constFind(w);
    if (dataIt == mWindowsData.constEnd()) {
        return;
    }

    Animation &animation = m_animations[w];
    animation.kind = AnimationKind::In;
    animation.timeLine.reset();
    animation.timeLine.setDirection(TimeLine::Forward);
    animation.timeLine.setDuration((*dataIt).fadeInDuration);
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

    auto dataIt = mWindowsData.constFind(w);
    if (dataIt == mWindowsData.constEnd()) {
        return;
    }

    if (w->isDeleted()) {
        w->refWindow();
    }

    Animation &animation = m_animations[w];
    animation.kind = AnimationKind::Out;
    animation.timeLine.reset();
    animation.timeLine.setDirection(TimeLine::Backward);
    animation.timeLine.setDuration((*dataIt).fadeOutDuration);
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
