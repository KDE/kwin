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

#include <kdebug.h>
#include <KDE/KConfigGroup>
#include <QtCore/QTimeLine>

namespace KWin
{

KWIN_EFFECT(slidingpopups, SlidingPopupsEffect)

SlidingPopupsEffect::SlidingPopupsEffect()
{
    mAtom = XInternAtom(display(), "_KDE_SLIDE", False);
    effects->registerPropertyType(mAtom, true);
    // TODO hackish way to announce support, make better after 4.0
    unsigned char dummy = 0;
    XChangeProperty(display(), rootWindow(), mAtom, mAtom, 8, PropModeReplace, &dummy, 1);
    connect(effects, SIGNAL(windowAdded(EffectWindow*)), this, SLOT(slotWindowAdded(EffectWindow*)));
    connect(effects, SIGNAL(windowClosed(EffectWindow*)), this, SLOT(slotWindowClosed(EffectWindow*)));
    connect(effects, SIGNAL(windowDeleted(EffectWindow*)), this, SLOT(slotWindowDeleted(EffectWindow*)));
    connect(effects, SIGNAL(propertyNotify(EffectWindow*,long)), this, SLOT(slotPropertyNotify(EffectWindow*,long)));
    reconfigure(ReconfigureAll);
}

SlidingPopupsEffect::~SlidingPopupsEffect()
{
    XDeleteProperty(display(), rootWindow(), mAtom);
    effects->registerPropertyType(mAtom, false);
}

void SlidingPopupsEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)
    KConfigGroup conf = effects->effectConfig("SlidingPopups");
    mFadeInTime = animationTime(conf, "SlideInTime", 250);
    mFadeOutTime = animationTime(conf, "SlideOutTime", 250);
    QHash< const EffectWindow*, QTimeLine* >::iterator it = mAppearingWindows.begin();
    while (it != mAppearingWindows.end()) {
        it.value()->setDuration(animationTime(mFadeInTime));
        ++it;
    }
    it = mDisappearingWindows.begin();
    while (it != mDisappearingWindows.end()) {
        it.value()->setDuration(animationTime(mFadeOutTime));
        ++it;
    }
    QHash< const EffectWindow*, Data >::iterator wIt = mWindowsData.begin();
    while (wIt != mWindowsData.end()) {
        wIt.value().fadeInDuration = mFadeInTime;
        wIt.value().fadeOutDuration = mFadeOutTime;
        ++wIt;
    }
}

void SlidingPopupsEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (!mAppearingWindows.isEmpty() || !mDisappearingWindows.isEmpty())
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_WITHOUT_FULL_REPAINTS;
    effects->prePaintScreen(data, time);
}

void SlidingPopupsEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    qreal progress = 1.0;
    bool appearing = false;
    if (mAppearingWindows.contains(w)) {
        mAppearingWindows[ w ]->setCurrentTime(mAppearingWindows[ w ]->currentTime() + time);
        if (mAppearingWindows[ w ]->currentValue() < 1) {
            data.setTransformed();
            progress = mAppearingWindows[ w ]->currentValue();
            appearing = true;
        } else {
            delete mAppearingWindows.take(w);
            w->setData(WindowForceBlurRole, false);
        }
    } else if (mDisappearingWindows.contains(w)) {

        mDisappearingWindows[ w ]->setCurrentTime(mDisappearingWindows[ w ]->currentTime() + time);
        progress = mDisappearingWindows[ w ]->currentValue();

        if (progress != 1.0) {
            data.setTransformed();
            w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DELETE);
        } else {
            delete mDisappearingWindows.take(w);
            w->unrefWindow();
        }
    }
    if (progress != 1.0) {
        const int start = mWindowsData[ w ].start;
        if (start != 0) {
            const QRect screenRect = effects->clientArea(FullScreenArea, w->screen(), effects->currentDesktop());
            // filter out window quads, but only if the window does not start from the edge
            switch(mWindowsData[ w ].from) {
            case West: {
                const double splitPoint = w->width() - (w->x() + w->width() - screenRect.x() - start) + w->width() * (appearing ? 1.0 - progress : progress);
                data.quads = data.quads.splitAtX(splitPoint);
                WindowQuadList filtered;
                foreach (const WindowQuad &quad, data.quads) {
                    if (quad.left() >= splitPoint) {
                        filtered << quad;
                    }
                }
                data.quads = filtered;
                break;
            }
            case North: {
                const double splitPoint = w->height() - (w->y() + w->height() - screenRect.y() - start) + w->height() * (appearing ? 1.0 - progress : progress);
                data.quads = data.quads.splitAtY(splitPoint);
                WindowQuadList filtered;
                foreach (const WindowQuad &quad, data.quads) {
                    if (quad.top() >= splitPoint) {
                        filtered << quad;
                    }
                }
                data.quads = filtered;
                break;
            }
            case East: {
                const double splitPoint = screenRect.x() + screenRect.width() - w->x() - start - w->width() * (appearing ? 1.0 - progress : progress);
                data.quads = data.quads.splitAtX(splitPoint);
                WindowQuadList filtered;
                foreach (const WindowQuad &quad, data.quads) {
                    if (quad.right() <= splitPoint) {
                        filtered << quad;
                    }
                }
                data.quads = filtered;
                break;
            }
            case South:
            default: {
                const double splitPoint = screenRect.y() + screenRect.height() - w->y() - start - w->height() * (appearing ? 1.0 - progress : progress);
                data.quads = data.quads.splitAtY(splitPoint);
                WindowQuadList filtered;
                foreach (const WindowQuad &quad, data.quads) {
                    if (quad.bottom() <= splitPoint) {
                        filtered << quad;
                    }
                }
                data.quads = filtered;
                break;
            }
            }
        }
    }
    effects->prePaintWindow(w, data, time);
}

void SlidingPopupsEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    bool animating = false;
    bool appearing = false;

    if (mAppearingWindows.contains(w)) {
        appearing = true;
        animating = true;
    } else if (mDisappearingWindows.contains(w) && w->isDeleted()) {
        appearing = false;
        animating = true;
    }

    if (animating) {
        qreal progress;
        if (appearing)
            progress = 1.0 - mAppearingWindows[ w ]->currentValue();
        else {
            if (mDisappearingWindows.contains(w))
                progress = mDisappearingWindows[ w ]->currentValue();
            else
                progress = 1.0;
        }
        const int start = mWindowsData[ w ].start;

        const QRect screenRect = effects->clientArea(FullScreenArea, w->screen(), w->desktop());
        int splitPoint = 0;
        switch(mWindowsData[ w ].from) {
        case West:
            data.xTranslate -= w->width() * progress;
            splitPoint = w->width() - (w->x() + w->width() - screenRect.x() - start);
            region = QRegion(w->x() + splitPoint, w->y(), w->width() - splitPoint, w->height());
            break;
        case North:
            data.yTranslate -= w->height() * progress;
            splitPoint = w->height() - (w->y() + w->height() - screenRect.y() - start);
            region = QRegion(w->x(), w->y() + splitPoint, w->width(), w->height() - splitPoint);
            break;
        case East:
            data.xTranslate += w->width() * progress;
            splitPoint = screenRect.x() + screenRect.width() - w->x() - start;
            region = QRegion(w->x(), w->y(), splitPoint, w->height());
            break;
        case South:
        default:
            data.yTranslate += w->height() * progress;
            splitPoint = screenRect.y() + screenRect.height() - w->y() - start;
            region = QRegion(w->x(), w->y(), w->width(), splitPoint);
        }
    }

    effects->paintWindow(w, mask, region, data);
}

void SlidingPopupsEffect::postPaintWindow(EffectWindow* w)
{
    if (mAppearingWindows.contains(w) || mDisappearingWindows.contains(w)) {
        w->addRepaintFull(); // trigger next animation repaint
    }
    effects->postPaintWindow(w);
}

void SlidingPopupsEffect::slotWindowAdded(EffectWindow *w)
{
    slotPropertyNotify(w, mAtom);
    if (w->isOnCurrentDesktop() && mWindowsData.contains(w)) {
        mAppearingWindows.insert(w, new QTimeLine(mWindowsData[ w ].fadeInDuration, this));
        mAppearingWindows[ w ]->setCurveShape(QTimeLine::EaseInOutCurve);

        // Tell other windowAdded() effects to ignore this window
        w->setData(WindowAddedGrabRole, QVariant::fromValue(static_cast<void*>(this)));
        w->setData(WindowForceBlurRole, true);

        w->addRepaintFull();
    }
}

void SlidingPopupsEffect::slotWindowClosed(EffectWindow* w)
{
    slotPropertyNotify(w, mAtom);
    if (w->isOnCurrentDesktop() && !w->isMinimized() && mWindowsData.contains(w)) {
        w->refWindow();
        delete mAppearingWindows.take(w);
        mDisappearingWindows.insert(w, new QTimeLine(mWindowsData[ w ].fadeOutDuration, this));
        mDisappearingWindows[ w ]->setCurveShape(QTimeLine::EaseInOutCurve);

        // Tell other windowClosed() effects to ignore this window
        w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void*>(this)));
        w->setData(WindowForceBlurRole, true);

        w->addRepaintFull();
    }
}

void SlidingPopupsEffect::slotWindowDeleted(EffectWindow* w)
{
    delete mAppearingWindows.take(w);
    delete mDisappearingWindows.take(w);
    mWindowsData.remove(w);
    effects->addRepaint(w->geometry());
}

void SlidingPopupsEffect::slotPropertyNotify(EffectWindow* w, long a)
{
    if (!w || a != mAtom)
        return;

    QByteArray data = w->readProperty(mAtom, mAtom, 32);

    if (data.length() < 1) {
        // Property was removed, thus also remove the effect for window
        delete mAppearingWindows.take(w);
        delete mDisappearingWindows.take(w);
        mWindowsData.remove(w);
        return;
    }

    long* d = reinterpret_cast< long* >(data.data());
    Data animData;
    animData.start = d[ 0 ];
    animData.from = (Position)d[ 1 ];
    if (data.length() >= (int)(sizeof(long) * 3)) {
        animData.fadeInDuration = d[2];
        if (data.length() >= (int)(sizeof(long) * 4))
            animData.fadeOutDuration = d[3];
        else
            animData.fadeOutDuration = d[2];
    } else {
        animData.fadeInDuration = animationTime(mFadeInTime);
        animData.fadeOutDuration = animationTime(mFadeOutTime);
    }
    const QRect screenRect = effects->clientArea(FullScreenArea, w->screen(), effects->currentDesktop());
    if (animData.start == -1) {
        switch (animData.from) {
        case West:
            animData.start = qMax(w->x() - screenRect.x(), 0);
            break;
        case North:
            animData.start = qMax(w->y() - screenRect.y(), 0);
            break;
        case East:
            animData.start = qMax(screenRect.x() + screenRect.width() - (w->x() + w->width()), 0);
            break;
        case South:
        default:
            animData.start = qMax(screenRect.y() + screenRect.height() - (w->y() + w->height()), 0);
            break;
        }
    }
    // sanitize
    int difference = 0;
    switch (animData.from) {
    case West:
        difference = w->x() - screenRect.x();
        break;
    case North:
        difference = w->y() - screenRect.y();
        break;
    case East:
        difference = w->x() + w->width() - (screenRect.x() + screenRect.width());
        break;
    case South:
    default:
        difference = w->y() + w->height() - (screenRect.y() + screenRect.height());
        break;
    }
    animData.start = qMax<int>(animData.start, difference);
    mWindowsData[ w ] = animData;
}

bool SlidingPopupsEffect::isActive() const
{
    return !mAppearingWindows.isEmpty() || !mDisappearingWindows.isEmpty();
}

} // namespace
