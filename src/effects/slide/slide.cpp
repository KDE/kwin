/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// own
#include "slide.h"

// KConfigSkeleton
#include "slideconfig.h"

#include <cmath>

namespace KWin
{

SlideEffect::SlideEffect()
{
    initConfig<SlideConfig>();
    reconfigure(ReconfigureAll);

    connect(effects, QOverload<int, int, EffectWindow *>::of(&EffectsHandler::desktopChanged),
            this, &SlideEffect::desktopChanged);
    connect(effects, QOverload<uint, QPointF, EffectWindow *>::of(&EffectsHandler::desktopChanging),
            this, &SlideEffect::desktopChanging);
    connect(effects, QOverload<>::of(&EffectsHandler::desktopChangingCancelled),
            this, &SlideEffect::desktopChangingCancelled);
    connect(effects, &EffectsHandler::windowAdded,
            this, &SlideEffect::windowAdded);
    connect(effects, &EffectsHandler::windowDeleted,
            this, &SlideEffect::windowDeleted);
    connect(effects, &EffectsHandler::numberDesktopsChanged,
            this, &SlideEffect::finishedSwitching);
    connect(effects, &EffectsHandler::screenAdded,
            this, &SlideEffect::finishedSwitching);
    connect(effects, &EffectsHandler::screenRemoved,
            this, &SlideEffect::finishedSwitching);

    m_currentPosition = effects->desktopGridCoords(effects->currentDesktop());
}

SlideEffect::~SlideEffect()
{
    finishedSwitching();
}

bool SlideEffect::supported()
{
    return effects->animationsSupported();
}

void SlideEffect::reconfigure(ReconfigureFlags)
{
    SlideConfig::self()->read();

    const qreal springConstant = 300.0 / effects->animationTimeFactor();
    const qreal dampingRatio = 1.1;

    m_motionX = SpringMotion(springConstant, dampingRatio);
    m_motionY = SpringMotion(springConstant, dampingRatio);

    m_hGap = SlideConfig::horizontalGap();
    m_vGap = SlideConfig::verticalGap();
    m_slideBackground = SlideConfig::slideBackground();
}

inline QRegion buildClipRegion(const QPoint &pos, int w, int h)
{
    const QSize screenSize = effects->virtualScreenSize();
    QRegion r = QRect(pos, screenSize);
    if (effects->optionRollOverDesktops()) {
        r |= (r & QRect(-w, 0, w, h)).translated(w, 0); // W
        r |= (r & QRect(w, 0, w, h)).translated(-w, 0); // E

        r |= (r & QRect(0, -h, w, h)).translated(0, h); // N
        r |= (r & QRect(0, h, w, h)).translated(0, -h); // S

        r |= (r & QRect(-w, -h, w, h)).translated(w, h); // NW
        r |= (r & QRect(w, -h, w, h)).translated(-w, h); // NE
        r |= (r & QRect(w, h, w, h)).translated(-w, -h); // SE
        r |= (r & QRect(-w, h, w, h)).translated(w, -h); // SW
    }
    return r;
}

void SlideEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    std::chrono::milliseconds timeDelta = std::chrono::milliseconds::zero();
    if (m_lastPresentTime.count()) {
        timeDelta = presentTime - m_lastPresentTime;
    }
    m_lastPresentTime = presentTime;

    if (m_state == State::ActiveAnimation) {
        m_motionX.advance(timeDelta);
        m_motionY.advance(timeDelta);
        const QSize virtualSpaceSize = effects->virtualScreenSize();
        m_currentPosition.setX(m_motionX.position() / virtualSpaceSize.width());
        m_currentPosition.setY(m_motionY.position() / virtualSpaceSize.height());
    }

    const int w = effects->desktopGridWidth();
    const int h = effects->desktopGridHeight();

    // Clipping
    m_paintCtx.visibleDesktops.clear();
    m_paintCtx.visibleDesktops.reserve(4); // 4 - maximum number of visible desktops
    bool includedX = false, includedY = false;
    for (int i = 1; i <= effects->numberOfDesktops(); i++) {
        if (effects->desktopGridCoords(i).x() % w == (int)(m_currentPosition.x()) % w) {
            includedX = true;
        } else if (effects->desktopGridCoords(i).x() % w == ((int)(m_currentPosition.x()) + 1) % w) {
            includedX = true;
        }
        if (effects->desktopGridCoords(i).y() % h == (int)(m_currentPosition.y()) % h) {
            includedY = true;
        } else if (effects->desktopGridCoords(i).y() % h == ((int)(m_currentPosition.y()) + 1) % h) {
            includedY = true;
        }

        if (includedX && includedY) {
            m_paintCtx.visibleDesktops << i;
        }
    }

    data.mask |= PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_BACKGROUND_FIRST;

    effects->prePaintScreen(data, presentTime);
}

void SlideEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    m_paintCtx.wrap = effects->optionRollOverDesktops();
    effects->paintScreen(mask, region, data);
}

QPoint SlideEffect::getDrawCoords(QPointF pos, EffectScreen *screen)
{
    QPoint c = QPoint();
    c.setX(pos.x() * (screen->geometry().width() + m_hGap));
    c.setY(pos.y() * (screen->geometry().height() + m_vGap));
    return c;
}

/**
 * Decide whether given window @p w should be transformed/translated.
 * @returns @c true if given window @p w should be transformed, otherwise @c false
 */
bool SlideEffect::isTranslated(const EffectWindow *w) const
{
    if (w->isOnAllDesktops()) {
        if (w->isDesktop()) {
            return m_slideBackground;
        }
        return false;
    } else if (w == m_movingWindow) {
        return false;
    }
    return true;
}

/**
 * Will a window be painted during this frame?
 */
bool SlideEffect::willBePainted(const EffectWindow *w) const
{
    if (w->isOnAllDesktops()) {
        return true;
    }
    if (w == m_movingWindow) {
        return true;
    }
    for (const int &id : std::as_const(m_paintCtx.visibleDesktops)) {
        if (w->isOnDesktop(id)) {
            return true;
        }
    }
    return false;
}

void SlideEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    data.setTransformed();
    effects->prePaintWindow(w, data, presentTime);
}

void SlideEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (!willBePainted(w)) {
        return;
    }

    if (!isTranslated(w)) {
        effects->paintWindow(
            w,
            mask,
            region,
            data);
        return;
    }

    const int gridWidth = effects->desktopGridWidth();
    const int gridHeight = effects->desktopGridHeight();

    QPointF drawPosition = forcePositivePosition(m_currentPosition);
    drawPosition = m_paintCtx.wrap ? constrainToDrawableRange(drawPosition) : drawPosition;

    // If we're wrapping, draw the desktop in the second position.
    const bool wrappingX = drawPosition.x() > gridWidth - 1;
    const bool wrappingY = drawPosition.y() > gridHeight - 1;

    const auto screens = effects->screens();

    for (int desktop : std::as_const(m_paintCtx.visibleDesktops)) {
        if (!w->isOnDesktop(desktop)) {
            continue;
        }
        QPointF desktopTranslation = QPointF(effects->desktopGridCoords(desktop)) - drawPosition;
        // Decide if that first desktop should be drawn at 0 or the higher position used for wrapping.
        if (effects->desktopGridCoords(desktop).x() == 0 && wrappingX) {
            desktopTranslation = QPointF(desktopTranslation.x() + gridWidth, desktopTranslation.y());
        }
        if (effects->desktopGridCoords(desktop).y() == 0 && wrappingY) {
            desktopTranslation = QPointF(desktopTranslation.x(), desktopTranslation.y() + gridHeight);
        }

        for (EffectScreen *screen : screens) {
            QPoint drawTranslation = getDrawCoords(desktopTranslation, screen);
            data += drawTranslation;

            const QRect screenArea = screen->geometry();
            const QRect damage = screenArea.translated(drawTranslation).intersected(screenArea);

            effects->paintWindow(
                w,
                mask,
                // Only paint the region that intersects the current screen and desktop.
                region.intersected(damage),
                data);

            // Undo the translation for the next screen. I know, it hurts me too.
            data += QPoint(-drawTranslation.x(), -drawTranslation.y());
        }
    }
}

void SlideEffect::postPaintScreen()
{
    if (m_state == State::ActiveAnimation && !m_motionX.isMoving() && !m_motionY.isMoving()) {
        finishedSwitching();
    }

    effects->addRepaintFull();
    effects->postPaintScreen();
}

/*
 * Negative desktop positions aren't allowed.
 */
QPointF SlideEffect::forcePositivePosition(QPointF p) const
{
    if (p.x() < 0) {
        p.setX(p.x() + std::ceil(-p.x() / effects->desktopGridWidth()) * effects->desktopGridWidth());
    }
    if (p.y() < 0) {
        p.setY(p.y() + std::ceil(-p.y() / effects->desktopGridHeight()) * effects->desktopGridHeight());
    }
    return p;
}

bool SlideEffect::shouldElevate(const EffectWindow *w) const
{
    // Static docks(i.e. this effect doesn't slide docks) should be elevated
    // so they can properly animate themselves when an user enters or leaves
    // a virtual desktop with a window in fullscreen mode.
    return w->isDock();
}

/*
 * This function is called when the desktop changes.
 * Called AFTER the gesture is released.
 * Sets up animation to round off to the new current desktop.
 */
void SlideEffect::startAnimation(int old, int current, EffectWindow *movingWindow)
{
    if (m_state == State::Inactive) {
        prepareSwitching();
    }

    m_state = State::ActiveAnimation;
    m_movingWindow = movingWindow;

    m_startPos = m_currentPosition;
    m_endPos = effects->desktopGridCoords(current);
    if (effects->optionRollOverDesktops()) {
        optimizePath();
    }

    const QSize virtualSpaceSize = effects->virtualScreenSize();
    m_motionX.setAnchor(m_endPos.x() * virtualSpaceSize.width());
    m_motionX.setPosition(m_startPos.x() * virtualSpaceSize.width());
    m_motionY.setAnchor(m_endPos.y() * virtualSpaceSize.height());
    m_motionY.setPosition(m_startPos.y() * virtualSpaceSize.height());

    effects->setActiveFullScreenEffect(this);
    effects->addRepaintFull();
}

void SlideEffect::prepareSwitching()
{
    const auto windows = effects->stackingOrder();
    m_windowData.reserve(windows.count());

    for (EffectWindow *w : windows) {
        m_windowData[w] = WindowData{
            .visibilityRef = EffectWindowVisibleRef(w, EffectWindow::PAINT_DISABLED_BY_DESKTOP),
        };

        if (shouldElevate(w)) {
            effects->setElevatedWindow(w, true);
            m_elevatedWindows << w;
        }
        w->setData(WindowForceBackgroundContrastRole, QVariant(true));
        w->setData(WindowForceBlurRole, QVariant(true));
    }
}

void SlideEffect::finishedSwitching()
{
    if (m_state == State::Inactive) {
        return;
    }
    const EffectWindowList windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        w->setData(WindowForceBackgroundContrastRole, QVariant());
        w->setData(WindowForceBlurRole, QVariant());
    }

    for (EffectWindow *w : std::as_const(m_elevatedWindows)) {
        effects->setElevatedWindow(w, false);
    }
    m_elevatedWindows.clear();

    m_windowData.clear();
    m_movingWindow = nullptr;
    m_state = State::Inactive;
    m_lastPresentTime = std::chrono::milliseconds::zero();
    effects->setActiveFullScreenEffect(nullptr);
    m_currentPosition = effects->desktopGridCoords(effects->currentDesktop());
}

void SlideEffect::desktopChanged(int old, int current, EffectWindow *with)
{
    if (effects->hasActiveFullScreenEffect() && effects->activeFullScreenEffect() != this) {
        m_currentPosition = effects->desktopGridCoords(effects->currentDesktop());
        return;
    }

    startAnimation(old, current, with);
}

void SlideEffect::desktopChanging(uint old, QPointF desktopOffset, EffectWindow *with)
{
    if (effects->hasActiveFullScreenEffect() && effects->activeFullScreenEffect() != this) {
        return;
    }

    if (m_state == State::Inactive) {
        prepareSwitching();
    }

    m_state = State::ActiveGesture;
    m_movingWindow = with;

    // Find desktop position based on animationDelta
    QPoint gridPos = effects->desktopGridCoords(old);
    m_currentPosition.setX(gridPos.x() + desktopOffset.x());
    m_currentPosition.setY(gridPos.y() + desktopOffset.y());

    if (effects->optionRollOverDesktops()) {
        m_currentPosition = forcePositivePosition(m_currentPosition);
    } else {
        m_currentPosition = moveInsideDesktopGrid(m_currentPosition);
    }

    effects->setActiveFullScreenEffect(this);
    effects->addRepaintFull();
}

void SlideEffect::desktopChangingCancelled()
{
    // If the fingers have been lifted and the current desktop didn't change, start animation
    // to move back to the original virtual desktop.
    if (effects->activeFullScreenEffect() == this) {
        startAnimation(effects->currentDesktop(), effects->currentDesktop(), nullptr);
    }
}

QPointF SlideEffect::moveInsideDesktopGrid(QPointF p)
{
    if (p.x() < 0) {
        p.setX(0);
    }
    if (p.y() < 0) {
        p.setY(0);
    }
    if (p.x() > effects->desktopGridWidth() - 1) {
        p.setX(effects->desktopGridWidth() - 1);
    }
    if (p.y() > effects->desktopGridHeight() - 1) {
        p.setY(effects->desktopGridHeight() - 1);
    }
    return p;
}

void SlideEffect::windowAdded(EffectWindow *w)
{
    if (m_state == State::Inactive) {
        return;
    }
    if (shouldElevate(w)) {
        effects->setElevatedWindow(w, true);
        m_elevatedWindows << w;
    }
    w->setData(WindowForceBackgroundContrastRole, QVariant(true));
    w->setData(WindowForceBlurRole, QVariant(true));

    m_windowData[w] = WindowData{
        .visibilityRef = EffectWindowVisibleRef(w, EffectWindow::PAINT_DISABLED_BY_DESKTOP),
    };
}

void SlideEffect::windowDeleted(EffectWindow *w)
{
    if (m_state == State::Inactive) {
        return;
    }
    if (w == m_movingWindow) {
        m_movingWindow = nullptr;
    }
    m_elevatedWindows.removeAll(w);
    m_windowData.remove(w);
}

/*
 * Find the fastest path between two desktops.
 * This function decides when it's better to wrap around the grid or not.
 * Only call if wrapping is enabled.
 */
void SlideEffect::optimizePath()
{
    int w = effects->desktopGridWidth();
    int h = effects->desktopGridHeight();

    // Keep coordinates as low as possible
    if (m_startPos.x() >= w && m_endPos.x() >= w) {
        m_startPos.setX(fmod(m_startPos.x(), w));
        m_endPos.setX(fmod(m_endPos.x(), w));
    }
    if (m_startPos.y() >= h && m_endPos.y() >= h) {
        m_startPos.setY(fmod(m_startPos.y(), h));
        m_endPos.setY(fmod(m_endPos.y(), h));
    }

    // Is there is a shorter possible route?
    // If the x distance to be traveled is more than half the grid width, it's faster to wrap.
    // To avoid negative coordinates, take the lower coordinate and raise.
    if (std::abs((m_startPos.x() - m_endPos.x())) > w / 2.0) {
        if (m_startPos.x() < m_endPos.x()) {
            while (m_startPos.x() < m_endPos.x()) {
                m_startPos.setX(m_startPos.x() + w);
            }
        } else {
            while (m_endPos.x() < m_startPos.x()) {
                m_endPos.setX(m_endPos.x() + w);
            }
        }
        // Keep coordinates as low as possible
        if (m_startPos.x() >= w && m_endPos.x() >= w) {
            m_startPos.setX(fmod(m_startPos.x(), w));
            m_endPos.setX(fmod(m_endPos.x(), w));
        }
    }

    // Same for y
    if (std::abs((m_endPos.y() - m_startPos.y())) > (double)h / (double)2) {
        if (m_startPos.y() < m_endPos.y()) {
            while (m_startPos.y() < m_endPos.y()) {
                m_startPos.setY(m_startPos.y() + h);
            }
        } else {
            while (m_endPos.y() < m_startPos.y()) {
                m_endPos.setY(m_endPos.y() + h);
            }
        }
        // Keep coordinates as low as possible
        if (m_startPos.y() >= h && m_endPos.y() >= h) {
            m_startPos.setY(fmod(m_startPos.y(), h));
            m_endPos.setY(fmod(m_endPos.y(), h));
        }
    }
}

/*
 * Takes the point and uses modulus to keep draw position within [0, desktopGridWidth]
 * The render loop will draw the first desktop (0) after the last one (at position desktopGridWidth) for the wrap animation.
 * This function finds the true fastest path, regardless of which direction the animation is already going;
 * I was a little upset about this limitation until I realized that MacOS can't even wrap desktops :)
 */
QPointF SlideEffect::constrainToDrawableRange(QPointF p)
{
    p.setX(fmod(p.x(), effects->desktopGridWidth()));
    p.setY(fmod(p.y(), effects->desktopGridHeight()));
    return p;
}

} // namespace KWin
