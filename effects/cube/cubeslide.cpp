/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

#include "cubeslide.h"

#include <kwinconfig.h>
#include <kconfiggroup.h>

#include <math.h>

#include <kwinglutils.h>

namespace KWin
{

KWIN_EFFECT(cubeslide, CubeSlideEffect)
KWIN_EFFECT_SUPPORTED(cubeslide, CubeSlideEffect::supported())

CubeSlideEffect::CubeSlideEffect()
    : windowMoving(false)
    , desktopChangedWhileMoving(false)
    , progressRestriction(0.0f)
{
    connect(effects, SIGNAL(desktopChanged(int,int)), this, SLOT(slotDesktopChanged(int,int)));
    connect(effects, SIGNAL(windowStepUserMovedResized(EffectWindow*,QRect)), this, SLOT(slotWindowStepUserMovedResized(EffectWindow*)));
    connect(effects, SIGNAL(windowFinishUserMovedResized(EffectWindow*)), this, SLOT(slotWindowFinishUserMovedResized(EffectWindow*)));
    reconfigure(ReconfigureAll);
}

CubeSlideEffect::~CubeSlideEffect()
{
}

bool CubeSlideEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing;
}

void CubeSlideEffect::reconfigure(ReconfigureFlags)
{
    KConfigGroup conf = effects->effectConfig("CubeSlide");
    rotationDuration = animationTime(conf, "RotationDuration", 500);
    timeLine.setCurveShape(QTimeLine::EaseInOutCurve);
    timeLine.setDuration(rotationDuration);
    dontSlidePanels = conf.readEntry("DontSlidePanels", true);
    dontSlideStickyWindows = conf.readEntry("DontSlideStickyWindows", false);
    usePagerLayout = conf.readEntry("UsePagerLayout", true);
    useWindowMoving = conf.readEntry("UseWindowMoving", false);
}

void CubeSlideEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (!slideRotations.empty()) {
        data.mask |= PAINT_SCREEN_TRANSFORMED | Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS | PAINT_SCREEN_BACKGROUND_FIRST;
        timeLine.setCurrentTime(timeLine.currentTime() + time);
        if (windowMoving && timeLine.currentTime() > progressRestriction * (qreal)timeLine.duration())
            timeLine.setCurrentTime(progressRestriction * (qreal)timeLine.duration());
        if (dontSlidePanels)
            panels.clear();
        stickyWindows.clear();
    }
    effects->prePaintScreen(data, time);
}

void CubeSlideEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    if (!slideRotations.empty()) {
#ifdef KWIN_HAVE_OPENGLES
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        paintSlideCube(mask, region, data);
        glCullFace(GL_BACK);
        paintSlideCube(mask, region, data);
        glDisable(GL_CULL_FACE);
#else
        glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glPushMatrix();
        paintSlideCube(mask, region, data);
        glPopMatrix();
        glCullFace(GL_BACK);
        glPushMatrix();
        paintSlideCube(mask, region, data);
        glPopMatrix();
        glDisable(GL_CULL_FACE);
        glPopAttrib();
#endif
        if (dontSlidePanels) {
            foreach (EffectWindow * w, panels) {
                WindowPaintData wData(w);
                effects->paintWindow(w, 0, QRegion(w->x(), w->y(), w->width(), w->height()), wData);
            }
        }
        foreach (EffectWindow * w, stickyWindows) {
            WindowPaintData wData(w);
            effects->paintWindow(w, 0, QRegion(w->x(), w->y(), w->width(), w->height()), wData);
        }
    } else
        effects->paintScreen(mask, region, data);
}

void CubeSlideEffect::paintSlideCube(int mask, QRegion region, ScreenPaintData& data)
{
    // slide cube only paints to desktops at a time
    // first the horizontal rotations followed by vertical rotations
    QRect rect = effects->clientArea(FullArea, effects->activeScreen(), effects->currentDesktop());
    float point = rect.width() / 2 * tan(45.0f * M_PI / 180.0f);
    cube_painting = true;
    painting_desktop = front_desktop;

    ScreenPaintData firstFaceData = data;
    ScreenPaintData secondFaceData = data;
    RotationData firstFaceRot = RotationData();
    RotationData secondFaceRot = RotationData();
    RotationDirection direction = slideRotations.head();
    int secondDesktop;
    switch(direction) {
    case Left:
        firstFaceRot.axis = RotationData::YAxis;
        secondFaceRot.axis = RotationData::YAxis;
        if (usePagerLayout)
            secondDesktop = effects->desktopToLeft(front_desktop, true);
        else {
            secondDesktop = front_desktop - 1;
            if (secondDesktop == 0)
                secondDesktop = effects->numberOfDesktops();
        }
        firstFaceRot.angle = 90.0f * timeLine.currentValue();
        secondFaceRot.angle = -90.0f * (1.0f - timeLine.currentValue());
        break;
    case Right:
        firstFaceRot.axis = RotationData::YAxis;
        secondFaceRot.axis = RotationData::YAxis;
        if (usePagerLayout)
            secondDesktop = effects->desktopToRight(front_desktop, true);
        else {
            secondDesktop = front_desktop + 1;
            if (secondDesktop > effects->numberOfDesktops())
                secondDesktop = 1;
        }
        firstFaceRot.angle = -90.0f * timeLine.currentValue();
        secondFaceRot.angle = 90.0f * (1.0f - timeLine.currentValue());
        break;
    case Upwards:
        firstFaceRot.axis = RotationData::XAxis;
        secondFaceRot.axis = RotationData::XAxis;
        secondDesktop = effects->desktopAbove(front_desktop, true);
        firstFaceRot.angle = -90.0f * timeLine.currentValue();
        secondFaceRot.angle = 90.0f * (1.0f - timeLine.currentValue());
        point = rect.height() / 2 * tan(45.0f * M_PI / 180.0f);
        break;
    case Downwards:
        firstFaceRot.axis = RotationData::XAxis;
        secondFaceRot.axis = RotationData::XAxis;
        secondDesktop = effects->desktopBelow(front_desktop, true);
        firstFaceRot.angle = 90.0f * timeLine.currentValue();
        secondFaceRot.angle = -90.0f * (1.0f - timeLine.currentValue());
        point = rect.height() / 2 * tan(45.0f * M_PI / 180.0f);
        break;
    default:
        // totally impossible
        return;
    }
    // front desktop
    firstFaceRot.xRotationPoint = rect.width() / 2;
    firstFaceRot.yRotationPoint = rect.height() / 2;
    firstFaceRot.zRotationPoint = -point;
    firstFaceData.rotation = &firstFaceRot;
    other_desktop = secondDesktop;
    firstDesktop = true;
    effects->paintScreen(mask, region, firstFaceData);
    // second desktop
    other_desktop = painting_desktop;
    painting_desktop = secondDesktop;
    firstDesktop = false;
    secondFaceRot.xRotationPoint = rect.width() / 2;
    secondFaceRot.yRotationPoint = rect.height() / 2;
    secondFaceRot.zRotationPoint = -point;
    secondFaceData.rotation = &secondFaceRot;
    effects->paintScreen(mask, region, secondFaceData);
    cube_painting = false;
    painting_desktop = effects->currentDesktop();
}

void CubeSlideEffect::prePaintWindow(EffectWindow* w,  WindowPrePaintData& data, int time)
{
    if (!slideRotations.empty() && cube_painting) {
        QRect rect = effects->clientArea(FullArea, effects->activeScreen(), painting_desktop);
        if (dontSlidePanels && w->isDock()) {
            w->setData(WindowForceBlurRole, QVariant(true));
            panels.insert(w);
        }
        if (!w->isManaged()) {
            w->setData(WindowForceBlurRole, QVariant(true));
            stickyWindows.insert(w);
        } else if (dontSlideStickyWindows && !w->isDock() &&
                  !w->isDesktop() && w->isOnAllDesktops()) {
            w->setData(WindowForceBlurRole, QVariant(true));
            stickyWindows.insert(w);
        }
        if (w->isOnDesktop(painting_desktop)) {
            if (w->x() < rect.x()) {
                data.quads = data.quads.splitAtX(-w->x());
            }
            if (w->x() + w->width() > rect.x() + rect.width()) {
                data.quads = data.quads.splitAtX(rect.width() - w->x());
            }
            if (w->y() < rect.y()) {
                data.quads = data.quads.splitAtY(-w->y());
            }
            if (w->y() + w->height() > rect.y() + rect.height()) {
                data.quads = data.quads.splitAtY(rect.height() - w->y());
            }
            w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
        } else if (w->isOnDesktop(other_desktop)) {
            RotationDirection direction = slideRotations.head();
            bool enable = false;
            if (w->x() < rect.x() &&
                    (direction == Left || direction == Right)) {
                data.quads = data.quads.splitAtX(-w->x());
                enable = true;
            }
            if (w->x() + w->width() > rect.x() + rect.width() &&
                    (direction == Left || direction == Right)) {
                data.quads = data.quads.splitAtX(rect.width() - w->x());
                enable = true;
            }
            if (w->y() < rect.y() &&
                    (direction == Upwards || direction == Downwards)) {
                data.quads = data.quads.splitAtY(-w->y());
                enable = true;
            }
            if (w->y() + w->height() > rect.y() + rect.height() &&
                    (direction == Upwards || direction == Downwards)) {
                data.quads = data.quads.splitAtY(rect.height() - w->y());
                enable = true;
            }
            if (enable) {
                data.setTransformed();
                data.setTranslucent();
                w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
            } else
                w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
        } else
            w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
    }
    effects->prePaintWindow(w, data, time);
}

void CubeSlideEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (!slideRotations.empty() && cube_painting) {
        if (dontSlidePanels && w->isDock())
            return;
        if (stickyWindows.contains(w))
            return;

        // filter out quads overlapping the edges
        QRect rect = effects->clientArea(FullArea, effects->activeScreen(), painting_desktop);
        if (w->isOnDesktop(painting_desktop)) {
            if (w->x() < rect.x()) {
                WindowQuadList new_quads;
                foreach (const WindowQuad & quad, data.quads) {
                    if (quad.right() > -w->x()) {
                        new_quads.append(quad);
                    }
                }
                data.quads = new_quads;
            }
            if (w->x() + w->width() > rect.x() + rect.width()) {
                WindowQuadList new_quads;
                foreach (const WindowQuad & quad, data.quads) {
                    if (quad.right() <= rect.width() - w->x()) {
                        new_quads.append(quad);
                    }
                }
                data.quads = new_quads;
            }
            if (w->y() < rect.y()) {
                WindowQuadList new_quads;
                foreach (const WindowQuad & quad, data.quads) {
                    if (quad.bottom() > -w->y()) {
                        new_quads.append(quad);
                    }
                }
                data.quads = new_quads;
            }
            if (w->y() + w->height() > rect.y() + rect.height()) {
                WindowQuadList new_quads;
                foreach (const WindowQuad & quad, data.quads) {
                    if (quad.bottom() <= rect.height() - w->y()) {
                        new_quads.append(quad);
                    }
                }
                data.quads = new_quads;
            }
        }
        // paint windows overlapping edges from other desktop
        if (w->isOnDesktop(other_desktop) && (mask & PAINT_WINDOW_TRANSFORMED)) {
            RotationDirection direction = slideRotations.head();
            if (w->x() < rect.x() &&
                    (direction == Left || direction == Right)) {
                WindowQuadList new_quads;
                data.xTranslate = rect.width();
                foreach (const WindowQuad & quad, data.quads) {
                    if (quad.right() <= -w->x()) {
                        new_quads.append(quad);
                    }
                }
                data.quads = new_quads;
            }
            if (w->x() + w->width() > rect.x() + rect.width() &&
                    (direction == Left || direction == Right)) {
                WindowQuadList new_quads;
                data.xTranslate = -rect.width();
                foreach (const WindowQuad & quad, data.quads) {
                    if (quad.right() > rect.width() - w->x()) {
                        new_quads.append(quad);
                    }
                }
                data.quads = new_quads;
            }
            if (w->y() < rect.y() &&
                    (direction == Upwards || direction == Downwards)) {
                WindowQuadList new_quads;
                data.yTranslate = rect.height();
                foreach (const WindowQuad & quad, data.quads) {
                    if (quad.bottom() <= -w->y()) {
                        new_quads.append(quad);
                    }
                }
                data.quads = new_quads;
            }
            if (w->y() + w->height() > rect.y() + rect.height() &&
                    (direction == Upwards || direction == Downwards)) {
                WindowQuadList new_quads;
                data.yTranslate = -rect.height();
                foreach (const WindowQuad & quad, data.quads) {
                    if (quad.bottom() > rect.height() - w->y()) {
                        new_quads.append(quad);
                    }
                }
                data.quads = new_quads;
            }
            if (firstDesktop)
                data.opacity *= timeLine.currentValue();
            else
                data.opacity *= (1.0 - timeLine.currentValue());
        }
    }
    effects->paintWindow(w, mask, region, data);
}

void CubeSlideEffect::postPaintScreen()
{
    effects->postPaintScreen();
    if (!slideRotations.empty()) {
        if (timeLine.currentValue() == 1.0) {
            RotationDirection direction = slideRotations.dequeue();
            switch(direction) {
            case Left:
                if (usePagerLayout)
                    front_desktop = effects->desktopToLeft(front_desktop, true);
                else {
                    front_desktop--;
                    if (front_desktop == 0)
                        front_desktop = effects->numberOfDesktops();
                }
                break;
            case Right:
                if (usePagerLayout)
                    front_desktop = effects->desktopToRight(front_desktop, true);
                else {
                    front_desktop++;
                    if (front_desktop > effects->numberOfDesktops())
                        front_desktop = 1;
                }
                break;
            case Upwards:
                front_desktop = effects->desktopAbove(front_desktop, true);
                break;
            case Downwards:
                front_desktop = effects->desktopBelow(front_desktop, true);
                break;
            }
            timeLine.setCurrentTime(0);
            if (slideRotations.count() == 1)
                timeLine.setCurveShape(QTimeLine::EaseOutCurve);
            else
                timeLine.setCurveShape(QTimeLine::LinearCurve);
            if (slideRotations.empty()) {
                foreach (EffectWindow * w, panels)
                w->setData(WindowForceBlurRole, QVariant(false));
                foreach (EffectWindow * w, stickyWindows)
                w->setData(WindowForceBlurRole, QVariant(false));
                stickyWindows.clear();
                panels.clear();
                effects->setActiveFullScreenEffect(0);
            }
        }
        effects->addRepaintFull();
    }
}

void CubeSlideEffect::slotDesktopChanged(int old, int current)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return;
    if (old > effects->numberOfDesktops()) {
        // number of desktops has been reduced -> no animation
        return;
    }
    if (windowMoving) {
        desktopChangedWhileMoving = true;
        progressRestriction = 1.0 - progressRestriction;
        effects->addRepaintFull();
        return;
    }
    bool activate = true;
    if (!slideRotations.empty()) {
        // last slide still in progress
        activate = false;
        RotationDirection direction = slideRotations.dequeue();
        slideRotations.clear();
        slideRotations.enqueue(direction);
        switch(direction) {
        case Left:
            if (usePagerLayout)
                old = effects->desktopToLeft(front_desktop, true);
            else {
                old = front_desktop - 1;
                if (old == 0)
                    old = effects->numberOfDesktops();
            }
            break;
        case Right:
            if (usePagerLayout)
                old = effects->desktopToRight(front_desktop, true);
            else {
                old = front_desktop + 1;
                if (old > effects->numberOfDesktops())
                    old = 1;
            }
            break;
        case Upwards:
            old = effects->desktopAbove(front_desktop, true);
            break;
        case Downwards:
            old = effects->desktopBelow(front_desktop, true);
            break;
        }
    }
    if (usePagerLayout) {
        // calculate distance in respect to pager
        QPoint diff = effects->desktopGridCoords(effects->currentDesktop()) - effects->desktopGridCoords(old);
        if (qAbs(diff.x()) > effects->desktopGridWidth() / 2) {
            int sign = -1 * (diff.x() / qAbs(diff.x()));
            diff.setX(sign *(effects->desktopGridWidth() - qAbs(diff.x())));
        }
        if (diff.x() > 0) {
            for (int i = 0; i < diff.x(); i++) {
                slideRotations.enqueue(Right);
            }
        } else if (diff.x() < 0) {
            diff.setX(-diff.x());
            for (int i = 0; i < diff.x(); i++) {
                slideRotations.enqueue(Left);
            }
        }
        if (qAbs(diff.y()) > effects->desktopGridHeight() / 2) {
            int sign = -1 * (diff.y() / qAbs(diff.y()));
            diff.setY(sign *(effects->desktopGridHeight() - qAbs(diff.y())));
        }
        if (diff.y() > 0) {
            for (int i = 0; i < diff.y(); i++) {
                slideRotations.enqueue(Downwards);
            }
        }
        if (diff.y() < 0) {
            diff.setY(-diff.y());
            for (int i = 0; i < diff.y(); i++) {
                slideRotations.enqueue(Upwards);
            }
        }
    } else {
        // ignore pager layout
        int left = old - current;
        if (left < 0)
            left = effects->numberOfDesktops() + left;
        int right = current - old;
        if (right < 0)
            right = effects->numberOfDesktops() + right;
        if (left < right) {
            for (int i = 0; i < left; i++) {
                slideRotations.enqueue(Left);
            }
        } else {
            for (int i = 0; i < right; i++) {
                slideRotations.enqueue(Right);
            }
        }
    }
    timeLine.setDuration((float)rotationDuration / (float)slideRotations.count());
    if (activate) {
        if (slideRotations.count() == 1)
            timeLine.setCurveShape(QTimeLine::EaseInOutCurve);
        else
            timeLine.setCurveShape(QTimeLine::EaseInCurve);
        effects->setActiveFullScreenEffect(this);
        timeLine.setCurrentTime(0);
        front_desktop = old;
        effects->addRepaintFull();
    }
}

void CubeSlideEffect::slotWindowStepUserMovedResized(EffectWindow* w)
{
    if (!useWindowMoving)
        return;
    if (w->isUserResize())
        return;
    const QPoint cursor = effects->cursorPos();
    const int horizontal = displayWidth() * 0.1;
    const int vertical = displayHeight() * 0.1;
    const QRect leftRect(0, displayHeight() * 0.1, horizontal, displayHeight() * 0.8);
    const QRect rightRect(displayWidth() - horizontal, displayHeight() * 0.1, horizontal, displayHeight() * 0.8);
    const QRect topRect(horizontal, 0, displayWidth() * 0.8, vertical);
    const QRect bottomRect(horizontal, displayHeight() - vertical, displayWidth() - horizontal * 2, vertical);
    if (leftRect.contains(cursor)) {
        if (effects->desktopToLeft(effects->currentDesktop()) != effects->currentDesktop())
            windowMovingChanged(0.3 *(float)(horizontal - cursor.x()) / (float)horizontal, Left);
    } else if (rightRect.contains(cursor)) {
        if (effects->desktopToRight(effects->currentDesktop()) != effects->currentDesktop())
            windowMovingChanged(0.3 *(float)(cursor.x() - displayWidth() + horizontal) / (float)horizontal, Right);
    } else if (topRect.contains(cursor)) {
        if (effects->desktopAbove(effects->currentDesktop()) != effects->currentDesktop())
            windowMovingChanged(0.3 *(float)(vertical - cursor.y()) / (float)vertical, Upwards);
    } else if (bottomRect.contains(cursor)) {
        if (effects->desktopBelow(effects->currentDesktop()) != effects->currentDesktop())
            windowMovingChanged(0.3 *(float)(cursor.y() - displayHeight() + vertical) / (float)vertical, Downwards);
    } else {
        // not in one of the areas
        windowMoving = false;
        desktopChangedWhileMoving = false;
        timeLine.setCurrentTime(0);
        if (!slideRotations.isEmpty())
            slideRotations.clear();
        effects->setActiveFullScreenEffect(0);
        effects->addRepaintFull();
    }
}

void CubeSlideEffect::slotWindowFinishUserMovedResized(EffectWindow* w)
{
    if (!useWindowMoving)
        return;
    if (w->isUserResize())
        return;
    if (!desktopChangedWhileMoving) {
        if (slideRotations.isEmpty())
            return;
        const RotationDirection direction = slideRotations.dequeue();
        switch(direction) {
        case Left:
            slideRotations.enqueue(Right);
            break;
        case Right:
            slideRotations.enqueue(Left);
            break;
        case Upwards:
            slideRotations.enqueue(Downwards);
            break;
        case Downwards:
            slideRotations.enqueue(Upwards);
            break;
        default:
            break; // impossible
        }
        timeLine.setCurrentTime(timeLine.duration() - timeLine.currentTime());
    }
    desktopChangedWhileMoving = false;
    windowMoving = false;
    effects->addRepaintFull();
}

void CubeSlideEffect::windowMovingChanged(float progress, RotationDirection direction)
{
    if (desktopChangedWhileMoving)
        progressRestriction = 1.0 - progress;
    else
        progressRestriction = progress;
    front_desktop = effects->currentDesktop();
    if (slideRotations.isEmpty()) {
        slideRotations.enqueue(direction);
        timeLine.setCurveShape(QTimeLine::EaseInOutCurve);
        windowMoving = true;
        effects->setActiveFullScreenEffect(this);
    }
    effects->addRepaintFull();
}

bool CubeSlideEffect::isActive() const
{
    return !slideRotations.isEmpty();
}

} // namespace
