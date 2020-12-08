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

namespace KWin
{

SlideEffect::SlideEffect()
{
    initConfig<SlideConfig>();
    reconfigure(ReconfigureAll);

    m_timeLine.setEasingCurve(QEasingCurve::OutCubic);

    connect(effects, QOverload<int, int, EffectWindow *>::of(&EffectsHandler::desktopChanged),
            this, &SlideEffect::desktopChanged);
    connect(effects, &EffectsHandler::windowAdded,
            this, &SlideEffect::windowAdded);
    connect(effects, &EffectsHandler::windowDeleted,
            this, &SlideEffect::windowDeleted);
    connect(effects, &EffectsHandler::numberDesktopsChanged,
            this, &SlideEffect::numberDesktopsChanged);
    connect(effects, &EffectsHandler::numberScreensChanged,
            this, &SlideEffect::numberScreensChanged);
}

SlideEffect::~SlideEffect()
{
    if (m_active) {
        stop();
    }
}

bool SlideEffect::supported()
{
    return effects->animationsSupported();
}

void SlideEffect::reconfigure(ReconfigureFlags)
{
    SlideConfig::self()->read();

    m_timeLine.setDuration(
        std::chrono::milliseconds(animationTime<SlideConfig>(500)));

    m_hGap = SlideConfig::horizontalGap();
    m_vGap = SlideConfig::verticalGap();
    m_slideDocks = SlideConfig::slideDocks();
    m_slideBackground = SlideConfig::slideBackground();
}

void SlideEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    m_timeLine.update(std::chrono::milliseconds(time));

    data.mask |= PAINT_SCREEN_TRANSFORMED
              |  PAINT_SCREEN_BACKGROUND_FIRST;

    effects->prePaintScreen(data, time);
}

/**
 * Wrap vector @p diff around grid @p w x @p h.
 *
 * Wrapping is done in such a way that magnitude of x and y component of vector
 * @p diff is less than half of @p w and half of @p h, respectively. This will
 * result in having the "shortest" path between two points.
 *
 * @param diff Vector between two points
 * @param w Width of the desktop grid
 * @param h Height of the desktop grid
 */
inline void wrapDiff(QPoint &diff, int w, int h)
{
    if (diff.x() > w/2) {
        diff.setX(diff.x() - w);
    } else if (diff.x() < -w/2) {
        diff.setX(diff.x() + w);
    }

    if (diff.y() > h/2) {
        diff.setY(diff.y() - h);
    } else if (diff.y() < -h/2) {
        diff.setY(diff.y() + h);
    }
}

inline QRegion buildClipRegion(const QPoint &pos, int w, int h)
{
    const QSize screenSize = effects->virtualScreenSize();
    QRegion r = QRect(pos, screenSize);
    if (effects->optionRollOverDesktops()) {
        r |= (r & QRect(-w, 0, w, h)).translated(w, 0);  // W
        r |= (r & QRect(w, 0, w, h)).translated(-w, 0);  // E

        r |= (r & QRect(0, -h, w, h)).translated(0, h);  // N
        r |= (r & QRect(0, h, w, h)).translated(0, -h);  // S

        r |= (r & QRect(-w, -h, w, h)).translated(w, h); // NW
        r |= (r & QRect(w, -h, w, h)).translated(-w, h); // NE
        r |= (r & QRect(w, h, w, h)).translated(-w, -h); // SE
        r |= (r & QRect(-w, h, w, h)).translated(w, -h); // SW
    }
    return r;
}

void SlideEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    const bool wrap = effects->optionRollOverDesktops();
    const int w = workspaceWidth();
    const int h = workspaceHeight();

    QPoint currentPos = m_startPos + m_diff * m_timeLine.value();

    // When "Desktop navigation wraps around" checkbox is checked, currentPos
    // can be outside the rectangle Rect{x:-w, y:-h, width:2*w, height: 2*h},
    // so we map currentPos back to the rect.
    if (wrap) {
        currentPos.setX(currentPos.x() % w);
        currentPos.setY(currentPos.y() % h);
    }

    QVector<int> visibleDesktops;
    visibleDesktops.reserve(4); // 4 - maximum number of visible desktops
    const QRegion clipRegion = buildClipRegion(currentPos, w, h);
    for (int i = 1; i <= effects->numberOfDesktops(); i++) {
        const QRect desktopGeo = desktopGeometry(i);
        if (!clipRegion.contains(desktopGeo)) {
            continue;
        }
        visibleDesktops << i;
    }

    // When we enter a virtual desktop that has a window in fullscreen mode,
    // stacking order is fine. When we leave a virtual desktop that has
    // a window in fullscreen mode, stacking order is no longer valid
    // because panels are raised above the fullscreen window. Construct
    // a list of fullscreen windows, so we can decide later whether
    // docks should be visible on different virtual desktops.
    if (m_slideDocks) {
        const auto windows = effects->stackingOrder();
        m_paintCtx.fullscreenWindows.clear();
        for (EffectWindow *w : windows) {
            if (!w->isFullScreen()) {
                continue;
            }
            m_paintCtx.fullscreenWindows << w;
        }
    }

    // If screen is painted with either PAINT_SCREEN_TRANSFORMED or
    // PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS there is no clipping!!
    // Push the screen geometry to the paint clipper so everything outside
    // of the screen geometry is clipped.
    PaintClipper pc(QRegion(effects->virtualScreenGeometry()));

    // Screen is painted in several passes. Each painting pass paints
    // a single virtual desktop. There could be either 2 or 4 painting
    // passes, depending how an user moves between virtual desktops.
    // Windows, such as docks or keep-above windows, are painted in
    // the last pass so they are above other windows.
    m_paintCtx.firstPass = true;
    const int lastDesktop = visibleDesktops.last();
    for (int desktop : qAsConst(visibleDesktops)) {
        m_paintCtx.desktop = desktop;
        m_paintCtx.lastPass = (lastDesktop == desktop);
        m_paintCtx.translation = desktopCoords(desktop) - currentPos;
        if (wrap) {
            wrapDiff(m_paintCtx.translation, w, h);
        }
        effects->paintScreen(mask, region, data);
        m_paintCtx.firstPass = false;
    }
}

/**
 * Decide whether given window @p w should be transformed/translated.
 * @returns @c true if given window @p w should be transformed, otherwise @c false
 */
bool SlideEffect::isTranslated(const EffectWindow *w) const
{
    if (w->isOnAllDesktops()) {
        if (w->isDock()) {
            return m_slideDocks;
        }
        if (w->isDesktop()) {
            return m_slideBackground;
        }
        return false;
    } else if (w == m_movingWindow) {
        return false;
    } else if (w->isOnDesktop(m_paintCtx.desktop)) {
        return true;
    }
    return false;
}

/**
 * Decide whether given window @p w should be painted.
 * @returns @c true if given window @p w should be painted, otherwise @c false
 */
bool SlideEffect::isPainted(const EffectWindow *w) const
{
    if (w->isOnAllDesktops()) {
        if (w->isDock()) {
            if (!m_slideDocks) {
                return m_paintCtx.lastPass;
            }
            for (const EffectWindow *fw : qAsConst(m_paintCtx.fullscreenWindows)) {
                if (fw->isOnDesktop(m_paintCtx.desktop)
                    && fw->screen() == w->screen()) {
                    return false;
                }
            }
            return true;
        }
        if (w->isDesktop()) {
            // If desktop background is not being slided, draw it only
            // in the first pass. Otherwise, desktop backgrounds from
            // follow-up virtual desktops will be drawn above windows
            // from previous virtual desktops.
            return m_slideBackground || m_paintCtx.firstPass;
        }
        // In order to make sure that 'keep above' windows are above
        // other windows during transition to another virtual desktop,
        // they should be painted in the last pass.
        if (w->keepAbove()) {
            return m_paintCtx.lastPass;
        }
        return true;
    } else if (w == m_movingWindow) {
        return m_paintCtx.lastPass;
    } else if (w->isOnDesktop(m_paintCtx.desktop)) {
        return true;
    }
    return false;
}

void SlideEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time)
{
    const bool painted = isPainted(w);
    if (painted) {
        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
    } else {
        w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
    }
    if (painted && isTranslated(w)) {
        data.setTransformed();
    }
    effects->prePaintWindow(w, data, time);
}

void SlideEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (isTranslated(w)) {
        data += m_paintCtx.translation;
    }
    effects->paintWindow(w, mask, region, data);
}

void SlideEffect::postPaintScreen()
{
    if (m_timeLine.done()) {
        stop();
    }

    effects->addRepaintFull();
    effects->postPaintScreen();
}

/**
 * Get position of the top-left corner of desktop @p id within desktop grid with gaps.
 * @param id ID of a virtual desktop
 */
QPoint SlideEffect::desktopCoords(int id) const
{
    QPoint c = effects->desktopCoords(id);
    const QPoint gridPos = effects->desktopGridCoords(id);
    c.setX(c.x() + m_hGap * gridPos.x());
    c.setY(c.y() + m_vGap * gridPos.y());
    return c;
}

/**
 * Get geometry of desktop @p id within desktop grid with gaps.
 * @param id ID of a virtual desktop
 */
QRect SlideEffect::desktopGeometry(int id) const
{
    QRect g = effects->virtualScreenGeometry();
    g.translate(desktopCoords(id));
    return g;
}

/**
 * Get width of a virtual desktop grid.
 */
int SlideEffect::workspaceWidth() const
{
    int w = effects->workspaceWidth();
    w += m_hGap * effects->desktopGridWidth();
    return w;
}

/**
 * Get height of a virtual desktop grid.
 */
int SlideEffect::workspaceHeight() const
{
    int h = effects->workspaceHeight();
    h += m_vGap * effects->desktopGridHeight();
    return h;
}

bool SlideEffect::shouldElevate(const EffectWindow *w) const
{
    // Static docks(i.e. this effect doesn't slide docks) should be elevated
    // so they can properly animate themselves when an user enters or leaves
    // a virtual desktop with a window in fullscreen mode.
    return w->isDock() && !m_slideDocks;
}

void SlideEffect::start(int old, int current, EffectWindow *movingWindow)
{
    m_movingWindow = movingWindow;

    const bool wrap = effects->optionRollOverDesktops();
    const int w = workspaceWidth();
    const int h = workspaceHeight();

    if (m_active) {
        QPoint passed = m_diff * m_timeLine.value();
        QPoint currentPos = m_startPos + passed;
        QPoint delta = desktopCoords(current) - desktopCoords(old);
        if (wrap) {
            wrapDiff(delta, w, h);
        }
        m_diff += delta - passed;
        m_startPos = currentPos;
        // TODO: Figure out how to smooth movement.
        m_timeLine.reset();
        return;
    }

    const auto windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        if (shouldElevate(w)) {
            effects->setElevatedWindow(w, true);
            m_elevatedWindows << w;
        }
        w->setData(WindowForceBackgroundContrastRole, QVariant(true));
        w->setData(WindowForceBlurRole, QVariant(true));
    }

    m_diff = desktopCoords(current) - desktopCoords(old);
    if (wrap) {
        wrapDiff(m_diff, w, h);
    }
    m_startPos = desktopCoords(old);
    m_timeLine.reset();
    m_active = true;
    effects->setActiveFullScreenEffect(this);
    effects->addRepaintFull();
}

void SlideEffect::stop()
{
    const EffectWindowList windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        w->setData(WindowForceBackgroundContrastRole, QVariant());
        w->setData(WindowForceBlurRole, QVariant());
    }

    for (EffectWindow *w : m_elevatedWindows) {
        effects->setElevatedWindow(w, false);
    }
    m_elevatedWindows.clear();

    m_paintCtx.fullscreenWindows.clear();
    m_movingWindow = nullptr;
    m_active = false;
    effects->setActiveFullScreenEffect(nullptr);
}

void SlideEffect::desktopChanged(int old, int current, EffectWindow *with)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this) {
        return;
    }
    start(old, current, with);
}

void SlideEffect::windowAdded(EffectWindow *w)
{
    if (!m_active) {
        return;
    }
    if (shouldElevate(w)) {
        effects->setElevatedWindow(w, true);
        m_elevatedWindows << w;
    }
    w->setData(WindowForceBackgroundContrastRole, QVariant(true));
    w->setData(WindowForceBlurRole, QVariant(true));
}

void SlideEffect::windowDeleted(EffectWindow *w)
{
    if (!m_active) {
        return;
    }
    if (w == m_movingWindow) {
        m_movingWindow = nullptr;
    }
    m_elevatedWindows.removeAll(w);
    m_paintCtx.fullscreenWindows.removeAll(w);
}

void SlideEffect::numberDesktopsChanged(uint)
{
    if (!m_active) {
        return;
    }
    stop();
}

void SlideEffect::numberScreensChanged()
{
    if (!m_active) {
        return;
    }
    stop();
}

} // namespace KWin
