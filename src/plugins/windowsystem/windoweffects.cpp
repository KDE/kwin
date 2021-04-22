/*
    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "windoweffects.h"
#include "effect_builtins.h"
#include "effects.h"

#include <QGuiApplication>
#include <QWidget>
#include <QWindow>

Q_DECLARE_METATYPE(KWindowEffects::SlideFromLocation)

namespace KWin
{

WindowEffects::WindowEffects()
    : QObject(),
      KWindowEffectsPrivate()
{
}

WindowEffects::~WindowEffects()
{}

bool WindowEffects::isEffectAvailable(KWindowEffects::Effect effect)
{
    if (!effects) {
        return false;
    }
    auto e = static_cast<EffectsHandlerImpl*>(effects);
    switch (effect) {
    case KWindowEffects::BackgroundContrast:
        return e->isEffectLoaded(BuiltInEffects::nameForEffect(BuiltInEffect::Contrast));
    case KWindowEffects::BlurBehind:
        return e->isEffectLoaded(BuiltInEffects::nameForEffect(BuiltInEffect::Blur));
    case KWindowEffects::Slide:
        return e->isEffectLoaded(BuiltInEffects::nameForEffect(BuiltInEffect::SlidingPopups));
    default:
        // plugin does not provide integration for other effects
        return false;
    }
}

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 81)
QList<QSize> WindowEffects::windowSizes(const QList<WId> &ids)
{
    Q_UNUSED(ids)
    return {};
}
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 82)
static QWindow *findWindow(WId win)
{
    const auto windows = qApp->allWindows();
    auto it = std::find_if(windows.begin(), windows.end(), [win] (QWindow *w) { return w->winId() == win; });
    if (it == windows.end()) {
        return nullptr;
    }
    return *it;
}
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 82)
void WindowEffects::slideWindow(WId id, KWindowEffects::SlideFromLocation location, int offset)
{
    slideWindow(findWindow(id), location, offset);
}
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 82)
void WindowEffects::presentWindows(WId controller, const QList<WId> &ids)
{
    Q_UNUSED(controller)
    Q_UNUSED(ids)
}
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 82)
void WindowEffects::presentWindows(WId controller, int desktop)
{
    Q_UNUSED(controller)
    Q_UNUSED(desktop)
}
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 82)
void WindowEffects::highlightWindows(WId controller, const QList<WId> &ids)
{
    Q_UNUSED(controller)
    Q_UNUSED(ids)
}
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 82)
void WindowEffects::enableBlurBehind(WId window, bool enable, const QRegion &region)
{
    enableBlurBehind(findWindow(window), enable, region);
}
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 82)
void WindowEffects::enableBackgroundContrast(WId window, bool enable, qreal contrast, qreal intensity, qreal saturation, const QRegion &region)
{
    enableBackgroundContrast(findWindow(window), enable, contrast, intensity, saturation, region);
}
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 67)
void WindowEffects::markAsDashboard(WId window)
{
    Q_UNUSED(window)
}
#endif

void WindowEffects::slideWindow(QWindow *window, KWindowEffects::SlideFromLocation location, int offset)
{
    if (!window) {
        return;
    }
    window->setProperty("kwin_slide", QVariant::fromValue(location));
    window->setProperty("kwin_slide_offset", offset);
}

void WindowEffects::enableBlurBehind(QWindow *window, bool enable, const QRegion &region)
{
    if (!window) {
        return;
    }
    if (enable) {
        window->setProperty("kwin_blur", region);
    } else {
        window->setProperty("kwin_blur", {});
    }
}

void WindowEffects::enableBackgroundContrast(QWindow *window, bool enable, qreal contrast, qreal intensity, qreal saturation, const QRegion &region)
{
    if (!window) {
        return;
    }
    if (enable) {
        window->setProperty("kwin_background_region", region);
        window->setProperty("kwin_background_contrast", contrast);
        window->setProperty("kwin_background_intensity", intensity);
        window->setProperty("kwin_background_saturation", saturation);
    } else {
        window->setProperty("kwin_background_region", {});
        window->setProperty("kwin_background_contrast", {});
        window->setProperty("kwin_background_intensity", {});
        window->setProperty("kwin_background_saturation", {});
    }
}

}
