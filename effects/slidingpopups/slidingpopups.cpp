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
    if (mAppearingWindows.contains(w)) {
        mAppearingWindows[ w ]->setCurrentTime(mAppearingWindows[ w ]->currentTime() + time);
        if (mAppearingWindows[ w ]->currentValue() < 1)
            data.setTransformed();
        else
            delete mAppearingWindows.take(w);
    } else if (mDisappearingWindows.contains(w)) {
        data.setTransformed();
        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DELETE);

        mDisappearingWindows[ w ]->setCurrentTime(mDisappearingWindows[ w ]->currentTime() + time);
    }
    effects->prePaintWindow(w, data, time);
}

void SlidingPopupsEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    bool animating = false;
    bool appearing = false;
    QRegion clippedRegion = region;

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
        QRect r;

        switch(mWindowsData[ w ].from) {
        case West:
            data.xTranslate += (start - w->width()) * progress;
            r = QRect(start - w->width(), w->y(), w->width(), w->height());
            break;
        case North:
            data.yTranslate += (start - w->height()) * progress;
            r = QRect(w->x(), start - w->height(), w->width(), w->height());
            break;
        case East:
            data.xTranslate += (start - w->x()) * progress;
            r = QRect(w->x() + w->width(), w->y(), w->width(), w->height());
            break;
        case South:
        default:
            data.yTranslate += (start - w->y()) * progress;
            r = QRect(w->x(), start, w->width(), w->height());
        }
        clippedRegion = clippedRegion.subtracted(r);
        effects->addRepaint(r);
    }

    effects->paintWindow(w, mask, clippedRegion, data);
}

void SlidingPopupsEffect::postPaintWindow(EffectWindow* w)
{
    if (mAppearingWindows.contains(w) || mDisappearingWindows.contains(w)) {
        w->addRepaintFull(); // trigger next animation repaint
        const int start = mWindowsData[ w ].start;
        switch(mWindowsData[ w ].from) {
        case West:
            effects->addRepaint(QRect(start, w->y(), w->x(), w->height()));
            break;
        case North:
            effects->addRepaint(QRect(w->x(), start, w->width(), w->y()));
            break;
        case East:
            effects->addRepaint(QRect(w->x() + w->width(), w->y(), displayWidth() - w->x() - w->width() - start, w->height()));
            break;
        case South:
        default:
            effects->addRepaint(QRect(w->x(), w->y()+w->height(), w->width(), displayHeight() - w->y() - w->height() - start));
        }
    }
    effects->postPaintWindow(w);
    if (mDisappearingWindows.contains(w) && mDisappearingWindows[ w ]->currentValue() >= 1) {
        delete mDisappearingWindows.take(w);
        w->unrefWindow();
        effects->addRepaint(w->geometry());
    }
}

void SlidingPopupsEffect::slotWindowAdded(EffectWindow *w)
{
    slotPropertyNotify(w, mAtom);
    if (w->isOnCurrentDesktop() && mWindowsData.contains(w)) {
        mAppearingWindows.insert(w, new QTimeLine(mWindowsData[ w ].fadeInDuration, this));
        mAppearingWindows[ w ]->setCurveShape(QTimeLine::EaseInOutCurve);

        // Tell other windowAdded() effects to ignore this window
        w->setData(WindowAddedGrabRole, QVariant::fromValue(static_cast<void*>(this)));

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
    mWindowsData[ w ] = animData;
}

bool SlidingPopupsEffect::isActive() const
{
    return !mAppearingWindows.isEmpty() || !mDisappearingWindows.isEmpty();
}

} // namespace
