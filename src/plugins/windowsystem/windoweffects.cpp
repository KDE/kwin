/*
    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "windoweffects.h"
#include "effects.h"

#include <QGuiApplication>
#include <QWidget>
#include <QWindow>

Q_DECLARE_METATYPE(KWindowEffects::SlideFromLocation)

namespace KWin
{

WindowEffects::WindowEffects()
    : QObject()
    , KWindowEffectsPrivate()
{
}

WindowEffects::~WindowEffects()
{
}

namespace
{
QWindow *findWindow(WId win)
{
    const auto windows = qApp->allWindows();
    auto it = std::find_if(windows.begin(), windows.end(), [win](QWindow *w) {
        return w->handle() && w->winId() == win;
    });
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
    auto e = static_cast<EffectsHandlerImpl *>(effects);
    switch (effect) {
    case KWindowEffects::BackgroundContrast:
        return e->isEffectLoaded(QStringLiteral("contrast"));
    case KWindowEffects::BlurBehind:
        return e->isEffectLoaded(QStringLiteral("blur"));
    case KWindowEffects::Slide:
        return e->isEffectLoaded(QStringLiteral("slidingpopups"));
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

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 81)
QList<QSize> WindowEffects::windowSizes(const QList<WId> &ids)
{
    return {};
}
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 82)
void WindowEffects::presentWindows(WId controller, const QList<WId> &ids)
{
}
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 82)
void WindowEffects::presentWindows(WId controller, int desktop)
{
}
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 82)
void WindowEffects::highlightWindows(WId controller, const QList<WId> &ids)
{
}
#endif

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
}
#endif

}
