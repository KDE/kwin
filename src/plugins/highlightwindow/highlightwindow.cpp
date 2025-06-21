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
{
    connect(effects, &EffectsHandler::windowAdded, this, &HighlightWindowEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &HighlightWindowEffect::slotWindowDeleted);

    m_highlightTimer.setInterval(1000 / 3);
    m_highlightTimer.setSingleShot(true);
    connect(&m_highlightTimer, &QTimer::timeout, this, &HighlightWindowEffect::highlight);

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

static std::chrono::milliseconds ns2ms(std::chrono::nanoseconds nsecs)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(nsecs);
}

void HighlightWindowEffect::highlightWindows(const QStringList &windows)
{
    QList<QUuid> ids;
    ids.reserve(windows.size());
    for (const QString &rawId : windows) {
        if (const QUuid id(rawId); id.isNull()) {
            qCWarning(KWIN_HIGHLIGHTWINDOW) << "Received invalid window id:" << rawId;
            return;
        } else {
            ids.append(id);
        }
    }
    scheduleHighlight(ids);
}

void HighlightWindowEffect::slotWindowAdded(EffectWindow *w)
{
    if (!m_highlightedWindows.isEmpty()) {
        if (isHighlightWindow(w)) {
            const quint64 animationId = startGhostAnimation(w); // this window is not currently highlighted
            complete(animationId);
        }
    }
}

void HighlightWindowEffect::slotWindowDeleted(EffectWindow *w)
{
    m_animations.remove(w);
    m_highlightedWindows.removeOne(w);
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

    m_highlightedWindows.clear();
}

void HighlightWindowEffect::highlight()
{
    QList<EffectWindow *> windows;
    windows.reserve(m_scheduledHighlightedWindows.count());
    for (const auto &window : m_scheduledHighlightedWindows) {
        if (auto effectWindow = effects->findWindow(window)) {
            windows.append(effectWindow);
        }
    }

    if (windows.isEmpty()) {
        finishHighlighting();
        return;
    }

    m_highlightedWindows = windows;
    m_highlightTimestamp = std::chrono::steady_clock::now();
    prepareHighlighting();
}

void HighlightWindowEffect::scheduleHighlight(const QList<QUuid> &windows)
{
    m_scheduledHighlightedWindows = windows;

    if (!m_highlightTimer.isActive()) {
        if (m_highlightTimestamp) {
            const auto diff = ns2ms(std::chrono::steady_clock::now() - *m_highlightTimestamp);
            if (diff >= m_highlightInterval) {
                highlight();
            } else {
                m_highlightTimer.start(diff);
            }
            return;
        }

        m_highlightTimer.start(m_highlightInterval);
    }
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
    const auto windows = arguments.first().value<QList<EffectWindow *>>();

    QList<QUuid> ids;
    ids.reserve(windows.count());
    for (const EffectWindow *window : windows) {
        ids << window->internalId();
    }

    scheduleHighlight(ids);
    return true;
}

} // namespace

#include "moc_highlightwindow.cpp"
