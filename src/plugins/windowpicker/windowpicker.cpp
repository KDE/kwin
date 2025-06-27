/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2025 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowpicker.h"

#include "effect/effecthandler.h"
#include "window.h"

#include <QPromise>
#include <QQuickItem>
#include <QTimer>

using namespace std::chrono_literals;

namespace KWin
{

WindowPicker::WindowPicker()
    : m_shutdownTimer(new QTimer(this))
{
    Q_ASSERT(effects);
    connect(effects, &EffectsHandler::screenAboutToLock, this, &WindowPicker::realDeactivate);
    m_shutdownTimer->setSingleShot(true);
    connect(m_shutdownTimer, &QTimer::timeout, this, &WindowPicker::realDeactivate);
    setSource(QUrl(QStringLiteral("qrc:/org.kde.kwin/windowpicker/qml/main.qml")));
    reconfigure(ReconfigureAll);
}

WindowPicker::~WindowPicker()
{
}

int WindowPicker::animationDuration() const
{
    return m_animationDuration;
}

void WindowPicker::setAnimationDuration(int duration)
{
    if (m_animationDuration != duration) {
        m_animationDuration = duration;
        Q_EMIT animationDurationChanged();
    }
}

int WindowPicker::requestedEffectChainPosition() const
{
    return 80;
}

void WindowPicker::reconfigure(ReconfigureFlags)
{
    setAnimationDuration(animationTime(300ms));
}

QFuture<EffectWindow *> WindowPicker::pickWindow(bool livePreviews)
{
    if (isRunning()) {
        return {};
    }
    if (effects->isScreenLocked()) {
        return {};
    }

    m_livePreviews = livePreviews;
    Q_EMIT livePreviewsChanged();

    setRunning(true);

    return m_promise.future();
}

void WindowPicker::windowPicked(Window *window)
{
    const auto screens = effects->screens();
    for (const auto screen : screens) {
        if (QuickSceneView *view = viewForScreen(screen)) {
            QMetaObject::invokeMethod(view->rootItem(), "stop");
        }
    }
    if (window) {
        m_promise.addResult(window->effectWindow());
        m_promise.finish();
    }

    m_promise = {};

    m_shutdownTimer->start(animationDuration());
}

void WindowPicker::realDeactivate()
{
    setRunning(false);
}

} // namespace KWin

#include "moc_windowpicker.cpp"
