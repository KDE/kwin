/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_EFFECT_BUILTINS_H
#define KWIN_EFFECT_BUILTINS_H
#include <kwineffects_export.h>
#include <QStringList>
#include <QUrl>
#include <functional>

namespace KWin
{
class Effect;

/**
 * Defines all the built in effects.
 */
enum class BuiltInEffect
{
    Invalid, ///< not a valid Effect
    Blur,
    ColorPicker,
    Contrast,
    CoverSwitch,
    Cube,
    CubeSlide,
    DesktopGrid,
    DimInactive,
    FallApart,
    FlipSwitch,
    Glide,
    HighlightWindow,
    Invert,
    Kscreen,
    LookingGlass,
    MagicLamp,
    Magnifier,
    MouseClick,
    MouseMark,
    PresentWindows,
    Resize,
    ScreenEdge,
    ScreenShot,
    Sheet,
    ShowFps,
    ShowPaint,
    Slide,
    SlideBack,
    SlidingPopups,
    SnapHelper,
    StartupFeedback,
    ThumbnailAside,
    TouchPoints,
    TrackMouse,
    WindowGeometry,
    WobblyWindows,
    Zoom
};

namespace BuiltInEffects
{

struct EffectData {
    QString name;
    QString displayName;
    QString comment;
    QString category;
    QString exclusiveCategory;
    QUrl video;
    bool enabled;
    bool internal;
    std::function<Effect*()> createFunction;
    std::function<bool()> supportedFunction;
    std::function<bool()> enabledFunction;
};

KWINEFFECTS_EXPORT Effect *create(BuiltInEffect effect);
KWINEFFECTS_EXPORT bool available(const QString &name);
KWINEFFECTS_EXPORT bool supported(BuiltInEffect effect);
KWINEFFECTS_EXPORT bool checkEnabledByDefault(BuiltInEffect effect);
KWINEFFECTS_EXPORT bool enabledByDefault(BuiltInEffect effect);
KWINEFFECTS_EXPORT QString nameForEffect(BuiltInEffect effect);
KWINEFFECTS_EXPORT BuiltInEffect builtInForName(const QString &name);
KWINEFFECTS_EXPORT QStringList availableEffectNames();
KWINEFFECTS_EXPORT QList<BuiltInEffect> availableEffects();
KWINEFFECTS_EXPORT const EffectData &effectData(BuiltInEffect effect);
}

}

#endif
