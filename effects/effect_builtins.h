/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
 **/
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
    DimScreen,
    FallApart,
    FlipSwitch,
    Glide,
    HighlightWindow,
    Invert,
    Kscreen,
    LookingGlass,
    MagicLamp,
    Magnifier,
    MinimizeAnimation,
    MouseClick,
    MouseMark,
    PresentWindows,
    Resize,
    Scale,
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
