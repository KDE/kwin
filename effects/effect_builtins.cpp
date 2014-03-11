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
    bool hasEffect(const QByteArray &name) const;
    bool supported(const QByteArray &name) const;
    bool enabledByDefault(const QByteArray &name) const;

private:
    typedef Effect *(*CreateInstanceFunction)();
    typedef bool (*SupportedFunction)();
    QHash<QByteArray, CreateInstanceFunction> m_createHash;
    QHash<QByteArray, SupportedFunction> m_supportedHash;
    QHash<QByteArray, SupportedFunction> m_enabledHash;
};

EffectLoader::EffectLoader()
{
#define EFFECT(name, className) \
    m_createHash.insert(QByteArrayLiteral(#name), &createHelper< className >);
    EFFECT(blur, BlurEffect)
    EFFECT(contrast, ContrastEffect)
    EFFECT(coverswitch, CoverSwitchEffect)
    EFFECT(cube, CubeEffect)
    EFFECT(cubeslide, CubeSlideEffect)
    EFFECT(dashboard, DashboardEffect)
    EFFECT(desktopgrid, DesktopGridEffect)
    EFFECT(diminactive, DimInactiveEffect)
    EFFECT(dimscreen, DimScreenEffect)
    EFFECT(fallapart, FallApartEffect)
    EFFECT(flipswitch, FlipSwitchEffect)
    EFFECT(glide, GlideEffect)
    EFFECT(highlightwindow, HighlightWindowEffect)
    EFFECT(invert, InvertEffect)
    EFFECT(kscreen, KscreenEffect)
    EFFECT(logout, LogoutEffect)
    EFFECT(lookingglass, LookingGlassEffect)
    EFFECT(magiclamp, MagicLampEffect)
    EFFECT(magnifier, MagnifierEffect)
    EFFECT(minimizeanimation, MinimizeAnimationEffect)
    EFFECT(mouseclick, MouseClickEffect)
    EFFECT(mousemark, MouseMarkEffect)
    EFFECT(presentwindows, PresentWindowsEffect)
    EFFECT(resize, ResizeEffect)
    EFFECT(screenedge, ScreenEdgeEffect)
    EFFECT(screenshot, ScreenShotEffect)
    EFFECT(sheet, SheetEffect)
    EFFECT(showfps, ShowFpsEffect)
    EFFECT(showpaint, ShowPaintEffect)
    EFFECT(slide, SlideEffect)
    EFFECT(slideback, SlideBackEffect)
    EFFECT(slidingpopups, SlidingPopupsEffect)
    EFFECT(snaphelper, SnapHelperEffect)
    EFFECT(startupfeedback, StartupFeedbackEffect)
    EFFECT(thumbnailaside, ThumbnailAsideEffect)
    EFFECT(trackmouse, TrackMouseEffect)
    EFFECT(windowgeometry, WindowGeometry)
    EFFECT(wobblywindows, WobblyWindowsEffect)
    EFFECT(zoom, ZoomEffect)

#undef EFFECT

#define SUPPORTED(name, method) \
    m_supportedHash.insert(QByteArrayLiteral(#name), &method);
    SUPPORTED(blur, BlurEffect::supported)
    SUPPORTED(contrast, ContrastEffect::supported)
    SUPPORTED(coverswitch, CoverSwitchEffect::supported)
    SUPPORTED(cube, CubeEffect::supported)
    SUPPORTED(cubeslide, CubeSlideEffect::supported)
    SUPPORTED(fallapart, FallApartEffect::supported)
    SUPPORTED(flipswitch, FlipSwitchEffect::supported)
    SUPPORTED(glide, GlideEffect::supported)
    SUPPORTED(invert, InvertEffect::supported)
    SUPPORTED(lookingglass, LookingGlassEffect::supported)
    SUPPORTED(magiclamp, MagicLampEffect::supported)
    SUPPORTED(magnifier, MagnifierEffect::supported)
    SUPPORTED(screenshot, ScreenShotEffect::supported)
    SUPPORTED(sheet, SheetEffect::supported)
    SUPPORTED(startupfeedback, StartupFeedbackEffect::supported)
    SUPPORTED(wobblywindows, WobblyWindowsEffect::supported)

#undef SUPPORTED

#define ENABLED(name, method) \
    m_enabledHash.insert(QByteArrayLiteral(#name), &method);
    ENABLED(blur, BlurEffect::enabledByDefault)
    ENABLED(contrast, ContrastEffect::enabledByDefault)

#undef ENABLED
}

Effect *EffectLoader::create(const QByteArray &name)
{
    auto it = m_createHash.constFind(name);
    if (it == m_createHash.constEnd()) {
        return nullptr;
    }
    return it.value()();
}

bool EffectLoader::hasEffect(const QByteArray &name) const
{
    return m_createHash.contains(name);
}

bool EffectLoader::supported(const QByteArray &name) const
{
    auto it = m_supportedHash.constFind(name);
    if (it != m_supportedHash.constEnd()) {
        return it.value()();
    }
    return true;
}

bool EffectLoader::enabledByDefault(const QByteArray &name) const
{
    auto it = m_enabledHash.constFind(name);
    if (it != m_enabledHash.constEnd()) {
        return it.value()();
    }
    return true;
}

Q_GLOBAL_STATIC(EffectLoader, s_effectLoader)

namespace BuiltInEffects
{

Effect *create(const QByteArray &name)
{
    return s_effectLoader->create(name);
}

bool available(const QByteArray &name)
{
    return s_effectLoader->hasEffect(name);
}

bool supported(const QByteArray &name)
{
    return s_effectLoader->supported(name);
}

bool enabledByDefault(const QByteArray &name)
{
    return s_effectLoader->enabledByDefault(name);
}

} // BuiltInEffects

} // namespace
