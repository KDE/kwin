/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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

#include "slide.h"
// KConfigSkeleton
#include "slideconfig.h"

#include <math.h>

namespace KWin
{

SlideEffect::SlideEffect()
    : slide(false)
{
    initConfig<SlideConfig>();
    connect(effects, SIGNAL(desktopChanged(int,int,KWin::EffectWindow*)),
            this, SLOT(slotDesktopChanged(int,int,KWin::EffectWindow*)));
    connect(effects, &EffectsHandler::windowAdded, this, &SlideEffect::windowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &SlideEffect::windowDeleted);
    mTimeLine.setCurveShape(QTimeLine::EaseInOutCurve);
    reconfigure(ReconfigureAll);
}

bool SlideEffect::supported()
{
    return effects->animationsSupported();
}

void SlideEffect::reconfigure(ReconfigureFlags)
{
    SlideConfig::self()->read();

    const auto d = animationTime(
        SlideConfig::duration() != 0 ? SlideConfig::duration() : 250);
    mTimeLine.setDuration(d);
}

void SlideEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (slide) {
        mTimeLine.setCurrentTime(mTimeLine.currentTime() + time);

        // PAINT_SCREEN_BACKGROUND_FIRST is needed because screen will be actually painted more than once,
        // so with normal screen painting second screen paint would erase parts of the first paint
        if (mTimeLine.currentValue() != 1)
            data.mask |= PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_BACKGROUND_FIRST;
        else {
            foreach (EffectWindow * w, effects->stackingOrder()) {
                w->setData(WindowForceBlurRole, QVariant(false));
                if (m_backgroundContrastForcedBefore.contains(w)) {
                    w->setData(WindowForceBackgroundContrastRole, QVariant());
                }
            }
            m_backgroundContrastForcedBefore.clear();
            m_movingWindow = nullptr;
            slide = false;
            mTimeLine.setCurrentTime(0);
            effects->setActiveFullScreenEffect(NULL);
        }
    }
    effects->prePaintScreen(data, time);
}

void SlideEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    if (slide && w != m_movingWindow) {
        if (w->isOnAllDesktops()) {
            bool keep_above = w->keepAbove() || w->isDock();
            if ((!slide_painting_sticky || keep_above) &&
                (!keep_above || !slide_painting_keep_above)) {
                w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
            }
        } else if (w->isOnDesktop(painting_desktop)) {
            data.setTransformed();
            w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
        } else {
            w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
        }
    }
    effects->prePaintWindow(w, data, time);
}

void SlideEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    if (mTimeLine.currentValue() == 0) {
        effects->paintScreen(mask, region, data);
        return;
    }

    /*
     Transformations are done by remembering starting position of the change and the progress
     of it, the destination is computed from the current desktop. Positions of desktops
     are done using their topleft corner.
    */
    QPoint destPos = desktopRect(effects->currentDesktop()).topLeft();
    QPoint diffPos = destPos - slide_start_pos;
    int w = 0;
    int h = 0;
    if (effects->optionRollOverDesktops()) {
        w = effects->workspaceWidth();
        h = effects->workspaceHeight();
        // wrap around if shorter
        if (diffPos.x() > 0 && diffPos.x() > w / 2)
            diffPos.setX(diffPos.x() - w);
        if (diffPos.x() < 0 && abs(diffPos.x()) > w / 2)
            diffPos.setX(diffPos.x() + w);
        if (diffPos.y() > 0 && diffPos.y() > h / 2)
            diffPos.setY(diffPos.y() - h);
        if (diffPos.y() < 0 && abs(diffPos.y()) > h / 2)
            diffPos.setY(diffPos.y() + h);
    }
    QPoint currentPos = slide_start_pos + mTimeLine.currentValue() * diffPos;
    QRegion currentRegion = QRect(currentPos, effects->virtualScreenSize());
    if (effects->optionRollOverDesktops()) {
        currentRegion |= (currentRegion & QRect(-w, 0, w, h)).translated(w, 0);
        currentRegion |= (currentRegion & QRect(0, -h, w, h)).translated(0, h);
        currentRegion |= (currentRegion & QRect(w, 0, w, h)).translated(-w, 0);
        currentRegion |= (currentRegion & QRect(0, h, w, h)).translated(0, -h);
    }
    bool do_sticky = true;
    // Assure that the windows that are on all desktops and always on top
    // are painted with the last screen (e.g. plasma's tooltips). All other windows
    // that are on all desktops (e.g. the background window) are painted together
    // with the first screen.
    int last_desktop = 0;
    QList<QRect> desktop_rects;
    for (int desktop = 1;
            desktop <= effects->numberOfDesktops();
            ++desktop) {
        QRect rect = desktopRect(desktop);
        desktop_rects << rect;
        if (currentRegion.contains(rect)) {
            last_desktop = desktop;
        }
    }
    for (int desktop = 1;
            desktop <= effects->numberOfDesktops();
            ++desktop) {
        QRect rect = desktop_rects[desktop-1];
        if (currentRegion.contains(rect)) {  // part of the desktop needs painting
            painting_desktop = desktop;
            slide_painting_sticky = do_sticky;
            slide_painting_keep_above = (last_desktop == desktop);
            slide_painting_diff = rect.topLeft() - currentPos;
            const QSize screenSize = effects->virtualScreenSize();
            if (effects->optionRollOverDesktops()) {
                if (slide_painting_diff.x() > screenSize.width())
                    slide_painting_diff.setX(slide_painting_diff.x() - w);
                if (slide_painting_diff.x() < -screenSize.width())
                    slide_painting_diff.setX(slide_painting_diff.x() + w);
                if (slide_painting_diff.y() > screenSize.height())
                    slide_painting_diff.setY(slide_painting_diff.y() - h);
                if (slide_painting_diff.y() < -screenSize.height())
                    slide_painting_diff.setY(slide_painting_diff.y() + h);
            }
            do_sticky = false; // paint on-all-desktop windows only once
            // TODO mask parts that are not visible?
            effects->paintScreen(mask, region, data);
        }
    }
}

void SlideEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (slide) {
        // Do not move a window if it is on all desktops or being moved to another desktop.
        if (!w->isOnAllDesktops() && w != m_movingWindow) {
            data += slide_painting_diff;
        }
    }
    effects->paintWindow(w, mask, region, data);
}

void SlideEffect::postPaintScreen()
{
    if (slide)
        effects->addRepaintFull();
    effects->postPaintScreen();
}

// Gives a position of the given desktop when all desktops are arranged in a grid
QRect SlideEffect::desktopRect(int desktop) const
{
    QRect rect = effects->virtualScreenGeometry();
    rect.translate(effects->desktopCoords(desktop));
    return rect;
}

void SlideEffect::slotDesktopChanged(int old, int current, EffectWindow* with)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return;

    if (slide) { // old slide still in progress
        QPoint diffPos = desktopRect(old).topLeft() - slide_start_pos;
        int w = 0;
        int h = 0;
        if (effects->optionRollOverDesktops()) {
            w = effects->workspaceWidth();
            h = effects->workspaceHeight();
            // wrap around if shorter
            if (diffPos.x() > 0 && diffPos.x() > w / 2)
                diffPos.setX(diffPos.x() - w);
            if (diffPos.x() < 0 && abs(diffPos.x()) > w / 2)
                diffPos.setX(diffPos.x() + w);
            if (diffPos.y() > 0 && diffPos.y() > h / 2)
                diffPos.setY(diffPos.y() - h);
            if (diffPos.y() < 0 && abs(diffPos.y()) > h / 2)
                diffPos.setY(diffPos.y() + h);
        }
        QPoint currentPos = slide_start_pos + mTimeLine.currentValue() * diffPos;
        const QSize screenSize = effects->virtualScreenSize();
        QRegion currentRegion = QRect(currentPos, screenSize);
        if (effects->optionRollOverDesktops()) {
            currentRegion |= (currentRegion & QRect(-w, 0, w, h)).translated(w, 0);
            currentRegion |= (currentRegion & QRect(0, -h, w, h)).translated(0, h);
            currentRegion |= (currentRegion & QRect(w, 0, w, h)).translated(-w, 0);
            currentRegion |= (currentRegion & QRect(0, h, w, h)).translated(0, -h);
        }
        QRect rect = desktopRect(current);
        if (currentRegion.contains(rect)) {
            // current position is in new current desktop (e.g. quickly changing back),
            // don't do full progress
            if (abs(currentPos.x() - rect.x()) > abs(currentPos.y() - rect.y()))
                mTimeLine.setCurrentTime((1.0 - abs(currentPos.x() - rect.x()) / double(screenSize.width()))*(qreal)mTimeLine.currentValue());
            else
                mTimeLine.setCurrentTime((1.0 - abs(currentPos.y() - rect.y()) / double(screenSize.height()))*(qreal)mTimeLine.currentValue());
        } else // current position is not on current desktop, do full progress
            mTimeLine.setCurrentTime(0);
        diffPos = rect.topLeft() - currentPos;
        if (mTimeLine.currentValue() <= 0) {
            // Compute starting point for this new move (given current and end positions)
            slide_start_pos = rect.topLeft() - diffPos * 1 / (1 - mTimeLine.currentValue());
        } else {
            // at the end, stop
            foreach (EffectWindow * w, m_backgroundContrastForcedBefore) {
                w->setData(WindowForceBackgroundContrastRole, QVariant());
            }
            m_backgroundContrastForcedBefore.clear();
            slide = false;
            mTimeLine.setCurrentTime(0);
            effects->setActiveFullScreenEffect(NULL);
        }
    } else {
        if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
            return;
        mTimeLine.setCurrentTime(0);
        slide_start_pos = desktopRect(old).topLeft();
        slide = true;
        foreach (EffectWindow * w, effects->stackingOrder()) {
            w->setData(WindowForceBlurRole, QVariant(true));
            if (shouldForceBackgroundContrast(w)) {
                m_backgroundContrastForcedBefore.append(w);
                w->setData(WindowForceBackgroundContrastRole, QVariant(true));
            }
        }
        effects->setActiveFullScreenEffect(this);
    }

    m_movingWindow = with;

    effects->addRepaintFull();
}

void SlideEffect::windowAdded(EffectWindow *w)
{
    if (slide && shouldForceBackgroundContrast(w)) {
        m_backgroundContrastForcedBefore.append(w);
        w->setData(WindowForceBackgroundContrastRole, QVariant(true));
    }
}

void SlideEffect::windowDeleted(EffectWindow *w)
{
    m_backgroundContrastForcedBefore.removeAll(w);
    if (w == m_movingWindow)
        m_movingWindow = nullptr;
}

bool SlideEffect::shouldForceBackgroundContrast(const EffectWindow *w) const
{
    // Windows that are docks, kept above (such as panel popups), and do not
    // have the background contrast explicitely disabled should be forced on
    // during the slide animation
    const bool bgWindow = (w->hasAlpha() && w->isOnAllDesktops() && (w->isDock() || w->keepAbove()));
    return bgWindow && (!w->data(WindowForceBackgroundContrastRole).isValid());
}

bool SlideEffect::isActive() const
{
    return slide;
}

} // namespace

