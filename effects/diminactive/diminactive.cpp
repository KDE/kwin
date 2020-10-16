/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// own
#include "diminactive.h"

// KConfigSkeleton
#include "diminactiveconfig.h"

namespace KWin
{

/**
 * Checks if two windows belong to the same window group
 *
 * One possible example of a window group is an app window and app
 * preferences window(e.g. Dolphin window and Dolphin Preferences window).
 *
 * @param w1 The first window
 * @param w2 The second window
 * @returns @c true if both windows belong to the same window group, @c false otherwise
 */
static inline bool belongToSameGroup(const EffectWindow *w1, const EffectWindow *w2)
{
    return w1 && w2 && w1->group() && w1->group() == w2->group();
}

DimInactiveEffect::DimInactiveEffect()
{
    initConfig<DimInactiveConfig>();
    reconfigure(ReconfigureAll);

    connect(effects, &EffectsHandler::windowActivated,
            this, &DimInactiveEffect::windowActivated);
    connect(effects, &EffectsHandler::windowClosed,
            this, &DimInactiveEffect::windowClosed);
    connect(effects, &EffectsHandler::windowDeleted,
            this, &DimInactiveEffect::windowDeleted);
    connect(effects, &EffectsHandler::activeFullScreenEffectChanged,
            this, &DimInactiveEffect::activeFullScreenEffectChanged);
    connect(effects, &EffectsHandler::windowKeepAboveChanged,
            this, &DimInactiveEffect::updateActiveWindow);
    connect(effects, &EffectsHandler::windowFullScreenChanged,
            this, &DimInactiveEffect::updateActiveWindow);
}

DimInactiveEffect::~DimInactiveEffect()
{
}

void DimInactiveEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    DimInactiveConfig::self()->read();

    // TODO: Use normalized strength param.
    m_dimStrength = DimInactiveConfig::strength() / 100.0;
    m_dimPanels = DimInactiveConfig::dimPanels();
    m_dimDesktop = DimInactiveConfig::dimDesktop();
    m_dimKeepAbove = DimInactiveConfig::dimKeepAbove();
    m_dimByGroup = DimInactiveConfig::dimByGroup();
    m_dimFullScreen = DimInactiveConfig::dimFullScreen();

    updateActiveWindow(effects->activeWindow());

    m_activeWindowGroup = (m_dimByGroup && m_activeWindow)
        ? m_activeWindow->group()
        : nullptr;

    m_fullScreenTransition.timeLine.setDuration(
        std::chrono::milliseconds(static_cast<int>(animationTime(250))));

    effects->addRepaintFull();
}

void DimInactiveEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    const std::chrono::milliseconds delta(time);

    if (m_fullScreenTransition.active) {
        m_fullScreenTransition.timeLine.update(delta);
    }

    auto transitionIt = m_transitions.begin();
    while (transitionIt != m_transitions.end()) {
        (*transitionIt).update(delta);
        ++transitionIt;
    }

    effects->prePaintScreen(data, time);
}

void DimInactiveEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    auto transitionIt = m_transitions.constFind(w);
    if (transitionIt != m_transitions.constEnd()) {
        const qreal transitionProgress = (*transitionIt).value();
        dimWindow(data, m_dimStrength * transitionProgress);
        effects->paintWindow(w, mask, region, data);
        return;
    }

    auto forceIt = m_forceDim.constFind(w);
    if (forceIt != m_forceDim.constEnd()) {
        const qreal forcedStrength = *forceIt;
        dimWindow(data, forcedStrength);
        effects->paintWindow(w, mask, region, data);
        return;
    }

    if (canDimWindow(w)) {
        dimWindow(data, m_dimStrength);
    }

    effects->paintWindow(w, mask, region, data);
}

void DimInactiveEffect::postPaintScreen()
{
    if (m_fullScreenTransition.active) {
        if (m_fullScreenTransition.timeLine.done()) {
            m_fullScreenTransition.active = false;
        }
        effects->addRepaintFull();
    }

    auto transitionIt = m_transitions.begin();
    while (transitionIt != m_transitions.end()) {
        EffectWindow *w = transitionIt.key();
        if ((*transitionIt).done()) {
            transitionIt = m_transitions.erase(transitionIt);
        } else {
            ++transitionIt;
        }
        w->addRepaintFull();
    }

    effects->postPaintScreen();
}

void DimInactiveEffect::dimWindow(WindowPaintData &data, qreal strength)
{
    qreal dimFactor;
    if (m_fullScreenTransition.active) {
        dimFactor = 1.0 - m_fullScreenTransition.timeLine.value();
    } else if (effects->activeFullScreenEffect()) {
        dimFactor = 0.0;
    } else {
        dimFactor = 1.0;
    }

    data.multiplyBrightness(1.0 - strength * dimFactor);
    data.multiplySaturation(1.0 - strength * dimFactor);
}

bool DimInactiveEffect::canDimWindow(const EffectWindow *w) const
{
    if (m_activeWindow == w) {
        return false;
    }

    if (m_dimByGroup && belongToSameGroup(m_activeWindow, w)) {
        return false;
    }

    if (w->isDock() && !m_dimPanels) {
        return false;
    }

    if (w->isDesktop() && !m_dimDesktop) {
        return false;
    }

    if (w->keepAbove() && !m_dimKeepAbove) {
        return false;
    }

    if (w->isFullScreen() && !m_dimFullScreen) {
        return false;
    }

    if (w->isPopupWindow()) {
        return false;
    }

    if (w->isX11Client() && !w->isManaged()) {
        return false;
    }

    return w->isNormalWindow()
        || w->isDialog()
        || w->isUtility()
        || w->isDock()
        || w->isDesktop();
}

void DimInactiveEffect::scheduleInTransition(EffectWindow *w)
{
    TimeLine &timeLine = m_transitions[w];
    timeLine.setDuration(
        std::chrono::milliseconds(static_cast<int>(animationTime(160))));
    if (timeLine.done()) {
        // If the Out animation is still active, then we're trucating
        // duration of the timeline(from 250ms to 160ms). If the timeline
        // is about to be finished with the old duration, then after
        // changing duration it will be in the "done" state. Thus, we
        // have to reset the timeline, otherwise it won't update progress.
        timeLine.reset();
    }
    timeLine.setDirection(TimeLine::Backward);
    timeLine.setEasingCurve(QEasingCurve::InOutSine);
}

void DimInactiveEffect::scheduleGroupInTransition(EffectWindow *w)
{
    if (!m_dimByGroup) {
        scheduleInTransition(w);
        return;
    }

    if (!w->group()) {
        scheduleInTransition(w);
        return;
    }

    const auto members = w->group()->members();
    for (EffectWindow *member : members) {
        scheduleInTransition(member);
    }
}

void DimInactiveEffect::scheduleOutTransition(EffectWindow *w)
{
    TimeLine &timeLine = m_transitions[w];
    timeLine.setDuration(
        std::chrono::milliseconds(static_cast<int>(animationTime(250))));
    if (timeLine.done()) {
        timeLine.reset();
    }
    timeLine.setDirection(TimeLine::Forward);
    timeLine.setEasingCurve(QEasingCurve::InOutSine);
}

void DimInactiveEffect::scheduleGroupOutTransition(EffectWindow *w)
{
    if (!m_dimByGroup) {
        scheduleOutTransition(w);
        return;
    }

    if (!w->group()) {
        scheduleOutTransition(w);
        return;
    }

    const auto members = w->group()->members();
    for (EffectWindow *member : members) {
        scheduleOutTransition(member);
    }
}

void DimInactiveEffect::scheduleRepaint(EffectWindow *w)
{
    if (!m_dimByGroup) {
        w->addRepaintFull();
        return;
    }

    if (!w->group()) {
        w->addRepaintFull();
        return;
    }

    const auto members = w->group()->members();
    for (EffectWindow *member : members) {
        member->addRepaintFull();
    }
}

void DimInactiveEffect::windowActivated(EffectWindow *w)
{
    if (!w) {
        return;
    }

    if (m_activeWindow == w) {
        return;
    }

    if (m_dimByGroup && belongToSameGroup(m_activeWindow, w)) {
        m_activeWindow = w;
        return;
    }

    // WORKAROUND: Deleted windows do not belong to any of window groups.
    // So, if one of windows in a window group is closed, the In transition
    // will be false-triggered for the rest of the window group. In addition
    // to the active window, keep track of active window group so we can
    // tell whether "focus" moved from a closed window to some other window
    // in a window group.
    if (m_dimByGroup && w->group() && w->group() == m_activeWindowGroup) {
        m_activeWindow = w;
        return;
    }

    EffectWindow *previousActiveWindow = m_activeWindow;
    m_activeWindow = canDimWindow(w) ? w : nullptr;

    m_activeWindowGroup = (m_dimByGroup && m_activeWindow)
        ? m_activeWindow->group()
        : nullptr;

    if (previousActiveWindow) {
        scheduleGroupOutTransition(previousActiveWindow);
        scheduleRepaint(previousActiveWindow);
    }

    if (m_activeWindow) {
        scheduleGroupInTransition(m_activeWindow);
        scheduleRepaint(m_activeWindow);
    }
}

void DimInactiveEffect::windowClosed(EffectWindow *w)
{
    // When a window is closed, we should force current dim strength that
    // is applied to it to avoid flickering when some effect animates
    // the disappearing of the window. If there is no such effect then
    // it won't be dimmed.
    qreal forcedStrength = 0.0;
    bool shouldForceDim = false;

    auto transitionIt = m_transitions.find(w);
    if (transitionIt != m_transitions.end()) {
        forcedStrength = m_dimStrength * (*transitionIt).value();
        shouldForceDim = true;
        m_transitions.erase(transitionIt);
    } else if (m_activeWindow == w) {
        forcedStrength = 0.0;
        shouldForceDim = true;
    } else if (m_dimByGroup && belongToSameGroup(m_activeWindow, w)) {
        forcedStrength = 0.0;
        shouldForceDim = true;
    } else if (canDimWindow(w)) {
        forcedStrength = m_dimStrength;
        shouldForceDim = true;
    }

    if (shouldForceDim) {
        m_forceDim.insert(w, forcedStrength);
    }

    if (m_activeWindow == w) {
        m_activeWindow = nullptr;
    }
}

void DimInactiveEffect::windowDeleted(EffectWindow *w)
{
    m_forceDim.remove(w);

    // FIXME: Sometimes we can miss the window close signal because KWin
    // can activate a window that is not ready for painting and the window
    // gets destroyed immediately. So, we have to remove active transitions
    // for that window here, otherwise we'll crash in postPaintScreen.
    m_transitions.remove(w);
}

void DimInactiveEffect::activeFullScreenEffectChanged()
{
    if (m_fullScreenTransition.timeLine.done()) {
        m_fullScreenTransition.timeLine.reset();
    }
    m_fullScreenTransition.timeLine.setDirection(
        effects->activeFullScreenEffect()
            ? TimeLine::Forward
            : TimeLine::Backward
    );
    m_fullScreenTransition.active = true;

    effects->addRepaintFull();
}

void DimInactiveEffect::updateActiveWindow(EffectWindow *w)
{
    if (effects->activeWindow() == nullptr) {
        return;
    }

    if (effects->activeWindow() != w) {
        return;
    }

    // Need to reset m_activeWindow because canDimWindow depends on it.
    m_activeWindow = nullptr;

    m_activeWindow = canDimWindow(w) ? w : nullptr;
}

} // namespace KWin
