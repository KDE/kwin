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
#include "effect_builtins.h"
// common effects
#include "backgroundcontrast/contrast.h"
#include "blur/blur.h"
#include "kscreen/kscreen.h"
#include "presentwindows/presentwindows.h"
#include "screenedge/screenedgeeffect.h"
#include "screenshot/screenshot.h"
#include "slidingpopups/slidingpopups.h"
// Common effects only relevant to desktop
#include "dashboard/dashboard.h"
#include "desktopgrid/desktopgrid.h"
#include "diminactive/diminactive.h"
#include "dimscreen/dimscreen.h"
#include "fallapart/fallapart.h"
#include "highlightwindow/highlightwindow.h"
#include "magiclamp/magiclamp.h"
#include "minimizeanimation/minimizeanimation.h"
#include "resize/resize.h"
#include "showfps/showfps.h"
#include "showpaint/showpaint.h"
#include "slide/slide.h"
#include "slideback/slideback.h"
#include "thumbnailaside/thumbnailaside.h"
#include "windowgeometry/windowgeometry.h"
#include "zoom/zoom.h"
#include "logout/logout.h"
// OpenGL-specific effects for desktop
#include "coverswitch/coverswitch.h"
#include "cube/cube.h"
#include "cube/cubeslide.h"
#include "flipswitch/flipswitch.h"
#include "glide/glide.h"
#include "invert/invert.h"
#include "lookingglass/lookingglass.h"
#include "magnifier/magnifier.h"
#include "mouseclick/mouseclick.h"
#include "mousemark/mousemark.h"
#include "sheet/sheet.h"
#include "snaphelper/snaphelper.h"
#include "startupfeedback/startupfeedback.h"
#include "trackmouse/trackmouse.h"
#include "wobblywindows/wobblywindows.h"

namespace KWin
{

template <class T>
inline Effect *createHelper()
{
    return new T();
}

class EffectLoader
{
public:
    EffectLoader();
    Effect *create(const QByteArray &name);
    Effect *create(BuiltInEffect effect);
    bool hasEffect(const QByteArray &name) const;
    bool supported(const QByteArray &name) const;
    bool supported(BuiltInEffect effect) const;
    bool enabledByDefault(const QByteArray &name) const;
    bool enabledByDefault(BuiltInEffect effect) const;
    QList<QByteArray> availableEffectNames() const;
    QList<BuiltInEffect> availableEffects() const;
    BuiltInEffect builtInForName(const QByteArray &name) const;
    QByteArray nameForEffect(BuiltInEffect effect) const;

private:
    typedef Effect *(*CreateInstanceFunction)();
    typedef bool (*SupportedFunction)();
    QHash<QByteArray, BuiltInEffect> m_effects;
    QMap<BuiltInEffect, CreateInstanceFunction> m_createHash;
    QMap<BuiltInEffect, SupportedFunction> m_supportedHash;
    QMap<BuiltInEffect, SupportedFunction> m_enabledHash;
};

EffectLoader::EffectLoader()
{
#define EFFECT(name, className) \
    m_effects.insert(QByteArrayLiteral(#name).toLower(), BuiltInEffect::name);\
    m_createHash.insert(BuiltInEffect::name, &createHelper< className >);
    EFFECT(Blur, BlurEffect)
    EFFECT(Contrast, ContrastEffect)
    EFFECT(CoverSwitch, CoverSwitchEffect)
    EFFECT(Cube, CubeEffect)
    EFFECT(CubeSlide, CubeSlideEffect)
    EFFECT(Dashboard, DashboardEffect)
    EFFECT(DesktopGrid, DesktopGridEffect)
    EFFECT(DimInactive, DimInactiveEffect)
    EFFECT(DimScreen, DimScreenEffect)
    EFFECT(FallApart, FallApartEffect)
    EFFECT(FlipSwitch, FlipSwitchEffect)
    EFFECT(Glide, GlideEffect)
    EFFECT(HighlightWindow, HighlightWindowEffect)
    EFFECT(Invert, InvertEffect)
    EFFECT(Kscreen, KscreenEffect)
    EFFECT(Logout, LogoutEffect)
    EFFECT(LookingGlass, LookingGlassEffect)
    EFFECT(MagicLamp, MagicLampEffect)
    EFFECT(Magnifier, MagnifierEffect)
    EFFECT(MinimizeAnimation, MinimizeAnimationEffect)
    EFFECT(MouseClick, MouseClickEffect)
    EFFECT(MouseMark, MouseMarkEffect)
    EFFECT(PresentWindows, PresentWindowsEffect)
    EFFECT(Resize, ResizeEffect)
    EFFECT(ScreenEdge, ScreenEdgeEffect)
    EFFECT(ScreenShot, ScreenShotEffect)
    EFFECT(Sheet, SheetEffect)
    EFFECT(ShowFps, ShowFpsEffect)
    EFFECT(ShowPaint, ShowPaintEffect)
    EFFECT(Slide, SlideEffect)
    EFFECT(SlideBack, SlideBackEffect)
    EFFECT(SlidingPopups, SlidingPopupsEffect)
    EFFECT(SnapHelper, SnapHelperEffect)
    EFFECT(StartupFeedback, StartupFeedbackEffect)
    EFFECT(ThumbnailAside, ThumbnailAsideEffect)
    EFFECT(TrackMouse, TrackMouseEffect)
    EFFECT(WindowGeometry, WindowGeometry)
    EFFECT(WobblyWindows, WobblyWindowsEffect)
    EFFECT(Zoom, ZoomEffect)

#undef EFFECT

#define SUPPORTED(name, method) \
    m_supportedHash.insert(BuiltInEffect::name, &method);
    SUPPORTED(Blur, BlurEffect::supported)
    SUPPORTED(Contrast, ContrastEffect::supported)
    SUPPORTED(CoverSwitch, CoverSwitchEffect::supported)
    SUPPORTED(Cube, CubeEffect::supported)
    SUPPORTED(CubeSlide, CubeSlideEffect::supported)
    SUPPORTED(FallApart, FallApartEffect::supported)
    SUPPORTED(FlipSwitch, FlipSwitchEffect::supported)
    SUPPORTED(Glide, GlideEffect::supported)
    SUPPORTED(Invert, InvertEffect::supported)
    SUPPORTED(LookingGlass, LookingGlassEffect::supported)
    SUPPORTED(MagicLamp, MagicLampEffect::supported)
    SUPPORTED(Magnifier, MagnifierEffect::supported)
    SUPPORTED(ScreenShot, ScreenShotEffect::supported)
    SUPPORTED(Sheet, SheetEffect::supported)
    SUPPORTED(StartupFeedback, StartupFeedbackEffect::supported)
    SUPPORTED(WobblyWindows, WobblyWindowsEffect::supported)

#undef SUPPORTED

#define ENABLED(name, method) \
    m_enabledHash.insert(BuiltInEffect::name, &method);
    ENABLED(Blur, BlurEffect::enabledByDefault)
    ENABLED(Contrast, ContrastEffect::enabledByDefault)

#undef ENABLED
}

Effect *EffectLoader::create(const QByteArray &name)
{
    return create(builtInForName(name));
}

Effect *EffectLoader::create(BuiltInEffect effect)
{
    auto it = m_createHash.constFind(effect);
    if (it == m_createHash.constEnd()) {
        return nullptr;
    }
    return it.value()();
}

bool EffectLoader::hasEffect(const QByteArray &name) const
{
    return m_effects.contains(name);
}

bool EffectLoader::supported(const QByteArray &name) const
{
    return supported(builtInForName(name));
}

bool EffectLoader::supported(BuiltInEffect effect) const
{
    if (effect == BuiltInEffect::Invalid) {
        return false;
    }
    auto it = m_supportedHash.constFind(effect);
    if (it != m_supportedHash.constEnd()) {
        return it.value()();
    }
    return true;
}

bool EffectLoader::enabledByDefault(const QByteArray &name) const
{
    return enabledByDefault(builtInForName(name));
}

bool EffectLoader::enabledByDefault(BuiltInEffect effect) const
{
    auto it = m_enabledHash.constFind(effect);
    if (it != m_enabledHash.constEnd()) {
        return it.value()();
    }
    return true;
}

QList< QByteArray > EffectLoader::availableEffectNames() const
{
    return m_effects.keys();
}

QList< BuiltInEffect > EffectLoader::availableEffects() const
{
    return m_effects.values();
}

BuiltInEffect EffectLoader::builtInForName(const QByteArray &name) const
{
    auto it = m_effects.find(name);
    if (it == m_effects.end()) {
        return BuiltInEffect::Invalid;
    }
    return it.value();
}

QByteArray EffectLoader::nameForEffect(BuiltInEffect effect) const
{
    return m_effects.key(effect);
}

Q_GLOBAL_STATIC(EffectLoader, s_effectLoader)

namespace BuiltInEffects
{

Effect *create(const QByteArray &name)
{
    return s_effectLoader->create(name);
}

Effect *create(BuiltInEffect effect)
{
    return s_effectLoader->create(effect);
}

bool available(const QByteArray &name)
{
    return s_effectLoader->hasEffect(name);
}

bool supported(const QByteArray &name)
{
    return s_effectLoader->supported(name);
}

bool supported(BuiltInEffect effect)
{
    return s_effectLoader->supported(effect);
}

bool enabledByDefault(const QByteArray &name)
{
    return s_effectLoader->enabledByDefault(name);
}

bool enabledByDefault(BuiltInEffect effect)
{
    return s_effectLoader->enabledByDefault(effect);
}

QList< QByteArray > availableEffectNames()
{
    return s_effectLoader->availableEffectNames();
}

QList< BuiltInEffect > availableEffects()
{
    return s_effectLoader->availableEffects();
}

BuiltInEffect builtInForName(const QByteArray &name)
{
    return s_effectLoader->builtInForName(name);
}

QByteArray nameForEffect(BuiltInEffect effect)
{
    return s_effectLoader->nameForEffect(effect);
}

} // BuiltInEffects

} // namespace
