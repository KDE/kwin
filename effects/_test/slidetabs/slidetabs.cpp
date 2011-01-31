/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Jorge Mata <matamax123@gmail.com>

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

#include "slidetabs.h"

#include <cmath>
#include <QPoint>

namespace KWin
{

KWIN_EFFECT(slidetabs, SlideTabsEffect)

SlideTabsEffect::SlideTabsEffect()
{
    reconfigure(ReconfigureAll);
}

void SlideTabsEffect::reconfigure(ReconfigureFlags)
{
    KConfigGroup conf = EffectsHandler::effectConfig("SlideTabs");
    switching = conf.readEntry("SlideSwitching", true);
    grouping = conf.readEntry("SlideGrouping", true);
    totalTime = conf.readEntry("SlideDuration", 500);
}

void SlideTabsEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time)
{
    if (motionManager.isManaging(w)) {
        data.setTransformed();
        w->enablePainting(EffectWindow::PAINT_DISABLED);
        timeLine.addTime(time);
    }
    effects->prePaintWindow(w, data, time);
}

void SlideTabsEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (motionManager.isManaging(w) && w == inMove)
        motionManager.apply(w, data);
    effects->paintWindow(w, mask, region, data);
}

void SlideTabsEffect::postPaintWindow(EffectWindow* w)
{
    if (motionManager.isManaging(w)) {
        if (w == inMove) {
            QRect moving = calculateNextMove();
            motionManager.moveWindow(w, moving);
            if (direction && timeLine.progress() >= 0.5) {
                moving = target;
                target = source;
                source = moving;
                direction = false;
                effects->setElevatedWindow(notMoving, false);
                effects->setElevatedWindow(w, true);
            } else if (timeLine.progress() >= 1.0) {
                effects->setElevatedWindow(notMoving, false);
                effects->setElevatedWindow(inMove, false);
                motionManager.unmanage(notMoving);
                motionManager.unmanage(inMove);
                notMoving = NULL;
                inMove = NULL;
                wasD = false;
                effects->addRepaintFull();
            }
        } else if (w == notMoving && !direction && target != w->geometry() && !wasD) {
            target = w->geometry();
        }
        w->addRepaintFull();
    }
    effects->postPaintWindow(w);
}

void SlideTabsEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    if (motionManager.managingWindows()) {
        motionManager.calculate(time);
        data.mask |= Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    }
    effects->prePaintScreen(data, time);
}

void SlideTabsEffect::postPaintScreen()
{
    if (motionManager.managingWindows())
        effects->addRepaintFull();
    effects->postPaintScreen();
}

void SlideTabsEffect::clientGroupItemSwitched(EffectWindow* from, EffectWindow* to)
{
    if (!switching)
        return;
    inMove = to;
    notMoving = from;
    source = notMoving->geometry();
    QRect window = notMoving->geometry();
    int leftSpace = window.x(), rightSpace = displayWidth() - (window.x() + window.width()),
        upSpace = window.y(), downSpace = displayHeight() - (window.y() + window.height());

    if (leftSpace >= rightSpace && leftSpace >= upSpace && leftSpace >= downSpace)
        target = QRect(source.x() - (1.2 * source.width()), source.y(), source.width(), source.height());
    else if (rightSpace >= leftSpace && rightSpace >= upSpace && rightSpace >= downSpace)
        target = QRect(source.x() + (1.2 * source.width()), source.y(), source.width(), source.height());
    else if (upSpace >= leftSpace && upSpace >= rightSpace && upSpace >= downSpace)
        target = QRect(source.x(), source.y() - (1.2 * source.height()), source.width(), source.height());
    else
        target = QRect(source.x(), source.y() + (1.2 * source.height()), source.width(), source.height());

    timeLine.setCurveShape(TimeLine::LinearCurve);
    timeLine.setDuration(animationTime(totalTime));
    timeLine.setProgress(0.0f);
    motionManager.manage(inMove);
    motionManager.manage(notMoving);
    distance = sqrt(((source.x() - target.x()) * (source.x() - target.x())) + ((source.y() - target.y()) * (source.y() - target.y())));
    effects->setElevatedWindow(notMoving, true);
    direction = wasD = true;

    QRect moving = calculateNextMove();
    motionManager.moveWindow(inMove, moving);
}

void SlideTabsEffect::clientGroupItemAdded(EffectWindow* from, EffectWindow* to)
{
    if (!grouping || from->desktop() != to->desktop() || from->isMinimized() || to->isMinimized())
        return;
    timeLine.setCurveShape(TimeLine::LinearCurve);
    timeLine.setDuration(animationTime(totalTime));
    timeLine.setProgress(0.0f);
    inMove = from;
    notMoving = to;
    source = inMove->geometry();
    target = notMoving->geometry();
    distance = sqrt(((source.x() - target.x()) * (source.x() - target.x())) + ((source.y() - target.y()) * (source.y() - target.y())));
    motionManager.manage(inMove);
    motionManager.manage(notMoving);
    QRect moving = calculateNextMove();
    motionManager.moveWindow(inMove, moving);
    effects->setElevatedWindow(notMoving, true);
    direction = wasD = false;
}

QPoint SlideTabsEffect::calculatePointTarget(const QPoint &a, const QPoint &b)
{
    double dy, dx, x, y, k;
    k = direction ? (2.0 * timeLine.progress()) : (wasD ? ((timeLine.progress() - 0.5) * 2) : timeLine.progress());
    dx = fabs(a.x() - b.x());
    dy = fabs(a.y() - b.y());
    y = k * dy;
    x = k * dx;
    if (a.x() > b.x())
        x = -x;
    if (a.y() > b.y())
        y = -y;
    return QPoint(a.x() + x, a.y() + y);
}

QRect SlideTabsEffect::calculateNextMove()
{
    QPoint topLeft, bottomRight;
    int mw = source.width(), mh = source.height(), tw = target.width(), th = target.height();
    topLeft = calculatePointTarget(QPoint(source.x(), source.y()), QPoint(target.x(), target.y()));
    bottomRight = calculatePointTarget(QPoint(source.x() + mw, source.y() + mh), QPoint(target.x() + tw, target.y() + th));
    return QRect(topLeft.x(), topLeft.y(), bottomRight.x() - topLeft.x(), bottomRight.y() - topLeft.y());
}

}
