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

#include <ranges>

using namespace std::chrono_literals;

Q_LOGGING_CATEGORY(KWIN_HIGHLIGHTWINDOW, "kwin_effect_highlightwindow", QtWarningMsg)

namespace KWin
{

HighlightWindowEffect::HighlightWindowEffect()
    : m_easingCurve(QEasingCurve::Linear)
    , m_fadeDuration(static_cast<int>(animationTime(150ms)))
{
    connect(effects, &EffectsHandler::windowAdded, this, &HighlightWindowEffect::slotWindowAdded);
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

static bool isHighlightWindow(const EffectWindow *window)
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
        if (isHighlightWindow(w)) {
            startGhostAnimation(w);
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
    QList<EffectWindow *> windows = effects->stackingOrder();
    windows.erase(std::remove_if(windows.begin(), windows.end(), [](const EffectWindow *window) {
        return !isHighlightWindow(window);
    }), windows.end());

    auto split = std::partition(windows.begin(), windows.end(), [this](const EffectWindow *window) {
        return isHighlighted(window);
    });

    for (auto it = windows.begin(); it != split; ++it) {
        startHighlightAnimation(*it);
    }

    for (auto it = split; it != windows.end(); ++it) {
        startGhostAnimation(*it);
    }

    if (!m_highlightedWindows.isEmpty()) {
        const bool constraint = std::ranges::all_of(m_highlightedWindows, [this](EffectWindow *window) {
            if (auto it = m_animations.constFind(window); it != m_animations.constEnd()) {
                return it->state == HighlightAnimation::State::Highlighted;
            }
            return true;
        });
        if (constraint) {
            for (const auto &[window, animation] : m_animations.asKeyValueRange()) {
                if (animation.state == HighlightAnimation::State::Ghost) {
                    animation.state = HighlightAnimation::State::Withdrawing;
                    animation.initialOpacity = animation.opacity;
                    animation.finalOpacity = 0.0;
                    animation.timeline = TimeLine(m_fadeDuration);
                    window->addRepaintFull();
                }
            }
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

    m_highlightedWindows.clear();
}

void HighlightWindowEffect::highlightWindows(const QList<KWin::EffectWindow *> &windows)
{
    if (windows.isEmpty()) {
        finishHighlighting();
        return;
    }

    m_highlightedWindows = windows;
    prepareHighlighting();
}

void HighlightWindowEffect::startGhostAnimation(EffectWindow *window)
{
    if (auto it = m_animations.find(window); it != m_animations.end()) {
        switch (it->state) {
        case HighlightAnimation::State::Ghost:
        case HighlightAnimation::State::Withdrawing:
        case HighlightAnimation::State::Withdrawn:
            break;
        case HighlightAnimation::State::Highlighting:
        case HighlightAnimation::State::Highlighted:
        case HighlightAnimation::State::Reverting:
        case HighlightAnimation::State::Initial:
            it->state = HighlightAnimation::State::Ghost;
            break;
        }
    } else {
        const bool hidden = isInitiallyHidden(window);

        m_animations[window] = HighlightAnimation{
            .state = hidden ? HighlightAnimation::State::Withdrawn : HighlightAnimation::State::Ghost,
            .timeline = TimeLine(m_fadeDuration),
            .finalOpacity = 0.0,
            .opacity = hidden ? 0.0 : 1.0,
        };
    }

    window->addRepaintFull();
}

void HighlightWindowEffect::startHighlightAnimation(EffectWindow *window)
{
    if (auto it = m_animations.find(window); it != m_animations.end()) {
        switch (it->state) {
        case HighlightAnimation::State::Initial:
        case HighlightAnimation::State::Ghost:
            if (it->opacity == 1.0) {
                it->state = HighlightAnimation::State::Highlighted;
                it->opacity = 1.0;
            } else {
                it->state = HighlightAnimation::State::Highlighting;
                it->timeline = TimeLine(m_fadeDuration);
                it->initialOpacity = it->opacity;
                it->finalOpacity = 1.0;
            }
            break;
        case HighlightAnimation::State::Highlighting:
        case HighlightAnimation::State::Highlighted:
            break;
        case HighlightAnimation::State::Withdrawing:
            it->state = HighlightAnimation::State::Highlighting;
            it->timeline = TimeLine(it->timeline.elapsed());
            it->initialOpacity = it->opacity;
            it->finalOpacity = 1.0;
            break;
        case HighlightAnimation::State::Withdrawn:
            it->state = HighlightAnimation::State::Highlighting;
            it->timeline = TimeLine(m_fadeDuration);
            it->initialOpacity = it->opacity;
            it->finalOpacity = 1.0;
            break;
        case HighlightAnimation::State::Reverting:
            it->state = HighlightAnimation::State::Highlighting;
            it->timeline = TimeLine(it->timeline.elapsed());
            it->initialOpacity = it->opacity;
            it->finalOpacity = 1.0;
            break;
        }

        window->elevate(true);
    } else {
        const bool hidden = isInitiallyHidden(window);

        m_animations[window] = HighlightAnimation{
            .state = hidden ? HighlightAnimation::State::Withdrawn : HighlightAnimation::State::Highlighted,
            .timeline = TimeLine(std::chrono::milliseconds(m_fadeDuration)),
            .finalOpacity = 1.0,
            .opacity = hidden ? 0.0 : 1.0,
        };
    }

    window->addRepaintFull();
}

void HighlightWindowEffect::startRevertAnimation(EffectWindow *window)
{
    for (const auto &[window, animation] : m_animations.asKeyValueRange()) {
        window->elevate(false);
    }
}

bool HighlightWindowEffect::isHighlighted(const EffectWindow *window) const
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

bool HighlightWindowEffect::isActive() const
{
    return !m_animations.isEmpty();
}

void HighlightWindowEffect::reconfigure(ReconfigureFlags flags)
{
    m_fadeDuration = std::chrono::milliseconds(static_cast<int>(animationTime(150ms)));
}

void HighlightWindowEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    for (const auto &[window, animation] : m_animations.asKeyValueRange()) {
        switch (animation.state) {
        case HighlightAnimation::State::Highlighting:
        case HighlightAnimation::State::Withdrawing:
        case HighlightAnimation::State::Reverting:
            animation.timeline.advance(presentTime);
            animation.opacity = std::lerp(animation.initialOpacity, animation.finalOpacity, animation.timeline.progress());
            break;

        case HighlightAnimation::State::Ghost:
        case HighlightAnimation::State::Highlighted:
        case HighlightAnimation::State::Withdrawn:
        case HighlightAnimation::State::Initial:
            break;
        }
    }

    effects->prePaintScreen(data, presentTime);
}

void HighlightWindowEffect::postPaintScreen()
{
    for (const auto &[window, animation] : m_animations.asKeyValueRange()) {
        switch (animation.state) {
        case HighlightAnimation::State::Ghost:
        case HighlightAnimation::State::Highlighted:
        case HighlightAnimation::State::Withdrawn:
        case HighlightAnimation::State::Initial:
            break;
        case HighlightAnimation::State::Highlighting:
            if (animation.timeline.done()) {
                animation.state = HighlightAnimation::State::Highlighted;
                animation.opacity = 1.0;
            }
            break;
        case HighlightAnimation::State::Withdrawing:
            if (animation.timeline.done()) {
                animation.state = HighlightAnimation::State::Withdrawn;
                animation.opacity = 0.0;
            }
            break;
        case HighlightAnimation::State::Reverting:
            if (animation.timeline.done()) {
                animation.state = HighlightAnimation::State::Initial;
                animation.opacity = isInitiallyHidden(window) ? 0.0 : 1.0;
            }
            break;
        }
    }

    if (!m_highlightedWindows.isEmpty()) {
        const bool constraint = std::ranges::all_of(m_highlightedWindows, [this](EffectWindow *window) {
            if (auto it = m_animations.constFind(window); it != m_animations.constEnd()) {
                return it->state == HighlightAnimation::State::Highlighted;
            }
            return true;
        });
        if (constraint) {
            for (const auto &[window, animation] : m_animations.asKeyValueRange()) {
                if (animation.state == HighlightAnimation::State::Ghost) {
                    animation.state = HighlightAnimation::State::Withdrawing;
                    animation.initialOpacity = animation.opacity;
                    animation.finalOpacity = 0.0;
                    animation.timeline = TimeLine(m_fadeDuration);
                }
            }
        }
    }

    for (auto it = m_animations.begin(); it != m_animations.end();) {
        EffectWindow *window = it.key();
        HighlightAnimation &animation = it.value();

        switch (animation.state) {
        case HighlightAnimation::State::Initial:
            it = m_animations.erase(it);
            break;
        case HighlightAnimation::State::Ghost:
        case HighlightAnimation::State::Highlighted:
        case HighlightAnimation::State::Withdrawn:
            ++it;
            break;
        case HighlightAnimation::State::Highlighting:
        case HighlightAnimation::State::Withdrawing:
        case HighlightAnimation::State::Reverting:
            ++it;
            window->addRepaintFull();
            break;
        }
    }

    effects->postPaintScreen();
}

void HighlightWindowEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (const auto it = m_animations.constFind(w); it != m_animations.constEnd()) {
        data.multiplyOpacity(it->opacity);
    }

    effects->paintWindow(renderTarget, viewport, w, mask, region, data);
}

} // namespace

#include "moc_highlightwindow.cpp"
