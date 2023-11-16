/*
    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "windoweffects.h"
#include "libkwineffects/effects.h"

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

bool WindowEffects::isEffectAvailable(KWindowEffects::Effect effect)
{
    if (!effects) {
        return false;
    }
    switch (effect) {
    case KWindowEffects::BackgroundContrast:
        return effects->isEffectLoaded(QStringLiteral("contrast"));
    case KWindowEffects::BlurBehind:
        return effects->isEffectLoaded(QStringLiteral("blur"));
    case KWindowEffects::Slide:
        return effects->isEffectLoaded(QStringLiteral("slidingpopups"));
    default:
        // plugin does not provide integration for other effects
        return false;
    }
}

void WindowEffects::slideWindow(QWindow *window, KWindowEffects::SlideFromLocation location, int offset)
{
    window->setProperty("kwin_slide", QVariant::fromValue(location));
    window->setProperty("kwin_slide_offset", offset);
}

void WindowEffects::enableBlurBehind(QWindow *window, bool enable, const QRegion &region)
{
    if (enable) {
        window->setProperty("kwin_blur", region);
    } else {
        window->setProperty("kwin_blur", {});
    }
}

void WindowEffects::enableBackgroundContrast(QWindow *window, bool enable, qreal contrast, qreal intensity, qreal saturation, const QRegion &region)
{
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
