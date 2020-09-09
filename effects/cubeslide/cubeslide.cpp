/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cubeslide.h"
// KConfigSkeleton
#include "cubeslideconfig.h"

#include <kwinconfig.h>
#include <kwinglutils.h>

#include <QVector3D>

#include <cmath>

namespace KWin
{

CubeSlideEffect::CubeSlideEffect()
    : stickyPainting(false)
    , windowMoving(false)
    , desktopChangedWhileMoving(false)
    , progressRestriction(0.0f)
{
    initConfig<CubeSlideConfig>();
    connect(effects, &EffectsHandler::windowAdded,
            this, &CubeSlideEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted,
            this, &CubeSlideEffect::slotWindowDeleted);
    connect(effects, QOverload<int,int,EffectWindow *>::of(&EffectsHandler::desktopChanged),
            this, &CubeSlideEffect::slotDesktopChanged);
    connect(effects, &EffectsHandler::windowStepUserMovedResized,
            this, &CubeSlideEffect::slotWindowStepUserMovedResized);
    connect(effects, &EffectsHandler::windowFinishUserMovedResized,
            this, &CubeSlideEffect::slotWindowFinishUserMovedResized);
    connect(effects, &EffectsHandler::numberDesktopsChanged,
            this, &CubeSlideEffect::slotNumberDesktopsChanged);
    reconfigure(ReconfigureAll);
}

CubeSlideEffect::~CubeSlideEffect()
{
}

bool CubeSlideEffect::supported()
{
    return effects->isOpenGLCompositing() && effects->animationsSupported();
}

void CubeSlideEffect::reconfigure(ReconfigureFlags)
{
    CubeSlideConfig::self()->read();
    // TODO: rename rotationDuration to duration
    rotationDuration = animationTime(CubeSlideConfig::rotationDuration() != 0 ? CubeSlideConfig::rotationDuration() : 500);
    timeLine.setEasingCurve(QEasingCurve::InOutSine);
    timeLine.setDuration(rotationDuration);
    dontSlidePanels = CubeSlideConfig::dontSlidePanels();
    dontSlideStickyWindows = CubeSlideConfig::dontSlideStickyWindows();
    usePagerLayout = CubeSlideConfig::usePagerLayout();
    useWindowMoving = CubeSlideConfig::useWindowMoving();
}

void CubeSlideEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (isActive()) {
        data.mask |= PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS | PAINT_SCREEN_BACKGROUND_FIRST;
        timeLine.setCurrentTime(timeLine.currentTime() + time);
        if (windowMoving && timeLine.currentTime() > progressRestriction * (qreal)timeLine.duration())
            timeLine.setCurrentTime(progressRestriction * (qreal)timeLine.duration());
    }
    effects->prePaintScreen(data, time);
}

void CubeSlideEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData& data)
{
    if (isActive()) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        paintSlideCube(mask, region, data);
        glCullFace(GL_BACK);
        paintSlideCube(mask, region, data);
        glDisable(GL_CULL_FACE);
        // Paint an extra screen with 'sticky' windows.
        if (!staticWindows.isEmpty()) {
            stickyPainting = true;
            effects->paintScreen(mask, region, data);
            stickyPainting = false;
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
    RotationDirection direction = slideRotations.head();
    int secondDesktop;
    switch(direction) {
    case Left:
        firstFaceData.setRotationAxis(Qt::YAxis);
        secondFaceData.setRotationAxis(Qt::YAxis);
        if (usePagerLayout)
            secondDesktop = effects->desktopToLeft(front_desktop, true);
        else {
            secondDesktop = front_desktop - 1;
            if (secondDesktop == 0)
                secondDesktop = effects->numberOfDesktops();
        }
        firstFaceData.setRotationAngle(90.0f * timeLine.currentValue());
        secondFaceData.setRotationAngle(-90.0f * (1.0f - timeLine.currentValue()));
        break;
    case Right:
        firstFaceData.setRotationAxis(Qt::YAxis);
        secondFaceData.setRotationAxis(Qt::YAxis);
        if (usePagerLayout)
            secondDesktop = effects->desktopToRight(front_desktop, true);
        else {
            secondDesktop = front_desktop + 1;
            if (secondDesktop > effects->numberOfDesktops())
                secondDesktop = 1;
        }
        firstFaceData.setRotationAngle(-90.0f * timeLine.currentValue());
        secondFaceData.setRotationAngle(90.0f * (1.0f - timeLine.currentValue()));
        break;
    case Upwards:
        firstFaceData.setRotationAxis(Qt::XAxis);
        secondFaceData.setRotationAxis(Qt::XAxis);
        secondDesktop = effects->desktopAbove(front_desktop, true);
        firstFaceData.setRotationAngle(-90.0f * timeLine.currentValue());
        secondFaceData.setRotationAngle(90.0f * (1.0f - timeLine.currentValue()));
        point = rect.height() / 2 * tan(45.0f * M_PI / 180.0f);
        break;
    case Downwards:
        firstFaceData.setRotationAxis(Qt::XAxis);
        secondFaceData.setRotationAxis(Qt::XAxis);
        secondDesktop = effects->desktopBelow(front_desktop, true);
        firstFaceData.setRotationAngle(90.0f * timeLine.currentValue());
        secondFaceData.setRotationAngle(-90.0f * (1.0f - timeLine.currentValue()));
        point = rect.height() / 2 * tan(45.0f * M_PI / 180.0f);
        break;
    default:
        // totally impossible
        return;
    }
    // front desktop
    firstFaceData.setRotationOrigin(QVector3D(rect.width() / 2, rect.height() / 2, -point));
    other_desktop = secondDesktop;
    firstDesktop = true;
    effects->paintScreen(mask, region, firstFaceData);
    // second desktop
    other_desktop = painting_desktop;
    painting_desktop = secondDesktop;
    firstDesktop = false;
    secondFaceData.setRotationOrigin(QVector3D(rect.width() / 2, rect.height() / 2, -point));
    effects->paintScreen(mask, region, secondFaceData);
    cube_painting = false;
    painting_desktop = effects->currentDesktop();
}

void CubeSlideEffect::prePaintWindow(EffectWindow* w,  WindowPrePaintData& data, int time)
{
    if (stickyPainting) {
        if (staticWindows.contains(w)) {
            w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
        } else {
            w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
        }
    } else if (isActive() && cube_painting) {
        if (staticWindows.contains(w)) {
            w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
            effects->prePaintWindow(w, data, time);
            return;
        }
        QRect rect = effects->clientArea(FullArea, effects->activeScreen(), painting_desktop);
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
    if (isActive() && cube_painting && !staticWindows.contains(w)) {
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
                data.setXTranslation(rect.width());
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
                data.setXTranslation(-rect.width());
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
                data.setYTranslation(rect.height());
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
                data.setYTranslation(-rect.height());
                foreach (const WindowQuad & quad, data.quads) {
                    if (quad.bottom() > rect.height() - w->y()) {
                        new_quads.append(quad);
                    }
                }
                data.quads = new_quads;
            }
            if (firstDesktop)
                data.multiplyOpacity(timeLine.currentValue());
            else
                data.multiplyOpacity((1.0 - timeLine.currentValue()));
        }
    }
    effects->paintWindow(w, mask, region, data);
}

void CubeSlideEffect::postPaintScreen()
{
    effects->postPaintScreen();
    if (isActive()) {
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
                timeLine.setEasingCurve(QEasingCurve::OutSine);
            else
                timeLine.setEasingCurve(QEasingCurve::Linear);
            if (slideRotations.empty()) {
                for (EffectWindow* w : staticWindows) {
                    w->setData(WindowForceBlurRole, QVariant());
                    w->setData(WindowForceBackgroundContrastRole, QVariant());
                }
                staticWindows.clear();
                effects->setActiveFullScreenEffect(nullptr);
            }
        }
        effects->addRepaintFull();
    }
}

void CubeSlideEffect::slotDesktopChanged(int old, int current, EffectWindow* w)
{
    Q_UNUSED(w)
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
        startAnimation();
        front_desktop = old;
        effects->addRepaintFull();
    }
}

void CubeSlideEffect::startAnimation() {
    const EffectWindowList windows = effects->stackingOrder();
    for (EffectWindow* w : windows) {
        if (!shouldAnimate(w)) {
            w->setData(WindowForceBlurRole, QVariant(true));
            w->setData(WindowForceBackgroundContrastRole, QVariant(true));
            staticWindows.insert(w);
        }
    }
    if (slideRotations.count() == 1) {
        timeLine.setEasingCurve(QEasingCurve::InOutSine);
    } else {
        timeLine.setEasingCurve(QEasingCurve::InSine);
    }
    effects->setActiveFullScreenEffect(this);
    timeLine.setCurrentTime(0);
}

void CubeSlideEffect::slotWindowAdded(EffectWindow* w) {
    if (!isActive()) {
        return;
    }
    if (!shouldAnimate(w)) {
        staticWindows.insert(w);
        w->setData(WindowForceBlurRole, QVariant(true));
        w->setData(WindowForceBackgroundContrastRole, QVariant(true));
    }
}

void CubeSlideEffect::slotWindowDeleted(EffectWindow* w) {
    staticWindows.remove(w);
}

bool CubeSlideEffect::shouldAnimate(const EffectWindow* w) const
{
    if (w->isDock()) {
        return !dontSlidePanels;
    }
    if (w->isOnAllDesktops()) {
        if (w->isDesktop()) {
            return true;
        }
        if (w->isSpecialWindow()) {
            return false;
        }
        return !dontSlideStickyWindows;
    }
    return true;
}

void CubeSlideEffect::slotWindowStepUserMovedResized(EffectWindow* w)
{
    if (!useWindowMoving)
        return;
    if (!effects->kwinOption(SwitchDesktopOnScreenEdgeMovingWindows).toBool())
        return;
    if (w->isUserResize())
        return;
    const QSize screenSize = effects->virtualScreenSize();
    const QPoint cursor = effects->cursorPos();
    const int horizontal = screenSize.width() * 0.1;
    const int vertical = screenSize.height() * 0.1;
    const QRect leftRect(0, screenSize.height() * 0.1, horizontal, screenSize.height() * 0.8);
    const QRect rightRect(screenSize.width() - horizontal, screenSize.height() * 0.1, horizontal, screenSize.height() * 0.8);
    const QRect topRect(horizontal, 0, screenSize.width() * 0.8, vertical);
    const QRect bottomRect(horizontal, screenSize.height() - vertical, screenSize.width() - horizontal * 2, vertical);
    if (leftRect.contains(cursor)) {
        if (effects->desktopToLeft(effects->currentDesktop()) != effects->currentDesktop())
            windowMovingChanged(0.3 *(float)(horizontal - cursor.x()) / (float)horizontal, Left);
    } else if (rightRect.contains(cursor)) {
        if (effects->desktopToRight(effects->currentDesktop()) != effects->currentDesktop())
            windowMovingChanged(0.3 *(float)(cursor.x() - screenSize.width() + horizontal) / (float)horizontal, Right);
    } else if (topRect.contains(cursor)) {
        if (effects->desktopAbove(effects->currentDesktop()) != effects->currentDesktop())
            windowMovingChanged(0.3 *(float)(vertical - cursor.y()) / (float)vertical, Upwards);
    } else if (bottomRect.contains(cursor)) {
        if (effects->desktopBelow(effects->currentDesktop()) != effects->currentDesktop())
            windowMovingChanged(0.3 *(float)(cursor.y() - screenSize.height() + vertical) / (float)vertical, Downwards);
    } else {
        // not in one of the areas
        windowMoving = false;
        desktopChangedWhileMoving = false;
        timeLine.setCurrentTime(0);
        if (!slideRotations.isEmpty())
            slideRotations.clear();
        effects->setActiveFullScreenEffect(nullptr);
        effects->addRepaintFull();
    }
}

void CubeSlideEffect::slotWindowFinishUserMovedResized(EffectWindow* w)
{
    if (!useWindowMoving)
        return;
    if (!effects->kwinOption(SwitchDesktopOnScreenEdgeMovingWindows).toBool())
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
        windowMoving = true;
        startAnimation();
    }
    effects->addRepaintFull();
}

bool CubeSlideEffect::isActive() const
{
    return !slideRotations.isEmpty();
}

void CubeSlideEffect::slotNumberDesktopsChanged()
{
    // This effect animates only aftermaths of desktop switching. There is no any
    // way to reference removed desktops for animation purposes. So our the best
    // shot is just to do nothing. It doesn't look nice and we probaby have to
    // find more proper way to handle this case.

    if (!isActive()) {
        return;
    }

    for (EffectWindow *w : staticWindows) {
        w->setData(WindowForceBlurRole, QVariant());
        w->setData(WindowForceBackgroundContrastRole, QVariant());
    }

    slideRotations.clear();
    staticWindows.clear();

    effects->setActiveFullScreenEffect(nullptr);
}

} // namespace
