/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "highlightwindow.h"
#include "effect/effecthandler.h"

#include <QDBusConnection>

using namespace std::chrono_literals;

Q_LOGGING_CATEGORY(KWIN_HIGHLIGHTWINDOW, "kwin_effect_highlightwindow", QtWarningMsg)

namespace KWin
{

HighlightWindowEffect::HighlightWindowEffect()
    : m_easingCurve(QEasingCurve::Linear)
    , m_fadeDuration(animationTime(150ms))
    , m_monitorWindow(nullptr)
{
    connect(effects, &EffectsHandler::windowAdded, this, &HighlightWindowEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &HighlightWindowEffect::slotWindowClosed);
    connect(effects, &EffectsHandler::windowDeleted, this, &HighlightWindowEffect::slotWindowDeleted);

    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/KWin/HighlightWindow"),
                                                 QStringLiteral("org.kde.KWin.HighlightWindow"),
                                                 this,
                                                 QDBusConnection::ExportScriptableContents);
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.KWin.HighlightWindow"));
}

HighlightWindowEffect::~HighlightWindowEffect()
{
    QDBusConnection::sessionBus().unregisterService(QStringLiteral("org.kde.KWin.HighlightWindow"));
}

static bool isInitiallyHidden(EffectWindow *w)
{
    // Is the window initially hidden until it is highlighted?
    return w->isMinimized() || !w->isOnCurrentDesktop();
}

static bool isHighlightWindow(EffectWindow *window)
{
    return window->isNormalWindow() || window->isDialog();
}

void HighlightWindowEffect::highlightWindows(const QStringList &windows)
{
    QList<EffectWindow *> effectWindows;
    effectWindows.reserve(windows.count());
    for (const auto &window : windows) {
        if (auto effectWindow = effects->findWindow(QUuid(window)); effectWindow) {
            effectWindows.append(effectWindow);
        } else if (auto effectWindow = effects->findWindow(window.toLong()); effectWindow) {
            effectWindows.append(effectWindow);
        }
    }
    highlightWindows(effectWindows);
}

void HighlightWindowEffect::slotWindowAdded(EffectWindow *w)
{
    if (!m_highlightedWindows.isEmpty()) {
        // On X11, the tabbox may ask us to highlight itself before the windowAdded signal
        // is emitted because override-redirect windows are shown after synthetic 50ms delay.
        if (m_highlightedWindows.contains(w)) {
            return;
        }
        // This window was demanded to be highlighted before it appeared on the screen.
        for (const WId &id : std::as_const(m_highlightedIds)) {
            if (w == effects->findWindow(id)) {
                const quint64 animationId = startHighlightAnimation(w);
                complete(animationId);
                return;
            }
        }
        if (isHighlightWindow(w)) {
            const quint64 animationId = startGhostAnimation(w); // this window is not currently highlighted
            complete(animationId);
        }
    }
}

void HighlightWindowEffect::slotWindowClosed(EffectWindow *w)
{
    if (m_monitorWindow == w) { // The monitoring window was destroyed
        finishHighlighting();
    }
}

void HighlightWindowEffect::slotWindowDeleted(EffectWindow *w)
{
    m_animations.remove(w);
}

void HighlightWindowEffect::prepareHighlighting()
{
    const QList<EffectWindow *> windows = effects->stackingOrder();
    for (EffectWindow *window : windows) {
        if (!isHighlightWindow(window)) {
            continue;
        }
        if (isHighlighted(window)) {
            startHighlightAnimation(window);
        } else {
            startGhostAnimation(window);
        }
    }
}

void HighlightWindowEffect::finishHighlighting()
{
    const QList<EffectWindow *> windows = effects->stackingOrder();
    for (EffectWindow *window : windows) {
        if (isHighlightWindow(window)) {
            startRevertAnimation(window);
        }
    }

    // Sanity check, ideally, this should never happen.
    if (!m_animations.isEmpty()) {
        for (quint64 &animationId : m_animations) {
            cancel(animationId);
        }
        m_animations.clear();
    }

    m_monitorWindow = nullptr;
    m_highlightedWindows.clear();
}

void HighlightWindowEffect::highlightWindows(const QList<KWin::EffectWindow *> &windows)
{
    if (windows.isEmpty()) {
        finishHighlighting();
        return;
    }

    m_monitorWindow = nullptr;
    m_highlightedWindows.clear();
    m_highlightedIds.clear();
    for (auto w : windows) {
        m_highlightedWindows << w;
    }
    prepareHighlighting();
}

quint64 HighlightWindowEffect::startGhostAnimation(EffectWindow *window)
{
    quint64 &animationId = m_animations[window];
    if (animationId) {
        retarget(animationId, FPx2(m_ghostOpacity, m_ghostOpacity), m_fadeDuration);
    } else {
        const qreal startOpacity = isInitiallyHidden(window) ? 0 : 1;
        animationId = set(window, Opacity, 0, m_fadeDuration, FPx2(m_ghostOpacity, m_ghostOpacity),
                          m_easingCurve, 0, FPx2(startOpacity, startOpacity), false, false);
    }
    return animationId;
}

quint64 HighlightWindowEffect::startHighlightAnimation(EffectWindow *window)
{
    quint64 &animationId = m_animations[window];
    if (animationId) {
        retarget(animationId, FPx2(1.0, 1.0), m_fadeDuration);
    } else {
        const qreal startOpacity = isInitiallyHidden(window) ? 0 : 1;
        animationId = set(window, Opacity, 0, m_fadeDuration, FPx2(1.0, 1.0),
                          m_easingCurve, 0, FPx2(startOpacity, startOpacity), false, false);
    }
    return animationId;
}

void HighlightWindowEffect::startRevertAnimation(EffectWindow *window)
{
    const quint64 animationId = m_animations.take(window);
    if (animationId) {
        const qreal startOpacity = isHighlighted(window) ? 1 : m_ghostOpacity;
        const qreal endOpacity = isInitiallyHidden(window) ? 0 : 1;
        animate(window, Opacity, 0, m_fadeDuration, FPx2(endOpacity, endOpacity),
                m_easingCurve, 0, FPx2(startOpacity, startOpacity), false, false);
        cancel(animationId);
    }
}

bool HighlightWindowEffect::isHighlighted(EffectWindow *window) const
{
    return m_highlightedWindows.contains(window);
}

bool HighlightWindowEffect::provides(Feature feature)
{
    switch (feature) {
    case HighlightWindows:
        return true;
    default:
        return false;
    }
}

bool HighlightWindowEffect::perform(Feature feature, const QVariantList &arguments)
{
    if (feature != HighlightWindows) {
        return false;
    }
    if (arguments.size() != 1) {
        return false;
    }
    highlightWindows(arguments.first().value<QList<EffectWindow *>>());
    return true;
}

} // namespace

#include "moc_highlightwindow.cpp"
