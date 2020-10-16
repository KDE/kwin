/*
    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "windoweffects.h"
#include "effect_builtins.h"
#include "../../effects.h"

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

namespace
{
QWindow *findWindow(WId win)
{
    const auto windows = qApp->allWindows();
    auto it = std::find_if(windows.begin(), windows.end(), [win] (QWindow *w) { return w->winId() == win; });
    if (it == windows.end()) {
        return nullptr;
    }
    return *it;
}
}

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

void WindowEffects::slideWindow(WId id, KWindowEffects::SlideFromLocation location, int offset)
{
    auto w = findWindow(id);
    if (!w) {
        return;
    }
    w->setProperty("kwin_slide", QVariant::fromValue(location));
    w->setProperty("kwin_slide_offset", offset);
}

#if KWINDOWSYSTEM_VERSION <= QT_VERSION_CHECK(5, 61, 0)
void WindowEffects::slideWindow(QWidget *widget, KWindowEffects::SlideFromLocation location)
{
    slideWindow(widget->winId(), location, 0);
}
#endif

QList<QSize> WindowEffects::windowSizes(const QList<WId> &ids)
{
    Q_UNUSED(ids)
    return {};
}

void WindowEffects::presentWindows(WId controller, const QList<WId> &ids)
{
    Q_UNUSED(controller)
    Q_UNUSED(ids)
}

void WindowEffects::presentWindows(WId controller, int desktop)
{
    Q_UNUSED(controller)
    Q_UNUSED(desktop)
}

void WindowEffects::highlightWindows(WId controller, const QList<WId> &ids)
{
    Q_UNUSED(controller)
    Q_UNUSED(ids)
}

void WindowEffects::enableBlurBehind(WId window, bool enable, const QRegion &region)
{
    auto w = findWindow(window);
    if (!w) {
        return;
    }
    if (enable) {
        w->setProperty("kwin_blur", region);
    } else {
        w->setProperty("kwin_blur", {});
    }
}

void WindowEffects::enableBackgroundContrast(WId window, bool enable, qreal contrast, qreal intensity, qreal saturation, const QRegion &region)
{
    auto w = findWindow(window);
    if (!w) {
        return;
    }
    if (enable) {
        w->setProperty("kwin_background_region", region);
        w->setProperty("kwin_background_contrast", contrast);
        w->setProperty("kwin_background_intensity", intensity);
        w->setProperty("kwin_background_saturation", saturation);
    } else {
        w->setProperty("kwin_background_region", {});
        w->setProperty("kwin_background_contrast", {});
        w->setProperty("kwin_background_intensity", {});
        w->setProperty("kwin_background_saturation", {});
    }
}

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 67)
void WindowEffects::markAsDashboard(WId window)
{
    Q_UNUSED(window)
}
#endif

}
