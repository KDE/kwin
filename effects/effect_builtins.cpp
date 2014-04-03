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

namespace BuiltInEffects
{

template <class T>
inline Effect *createHelper()
{
    return new T();
}

struct EffectData {
    QString name;
    bool enabled;
    std::function<Effect*()> createFunction;
    std::function<bool()> supportedFunction;
    std::function<bool()> enabledFunction;
};

static const QVector<EffectData> s_effectData = {
    {
        QString(),
        false,
        nullptr,
        nullptr,
        nullptr
    }, {
        QStringLiteral("blur"),
        true,
        &createHelper<BlurEffect>,
        &BlurEffect::supported,
        &BlurEffect::enabledByDefault
    }, {
        QStringLiteral("contrast"),
        true,
        &createHelper<ContrastEffect>,
        &ContrastEffect::supported,
        &ContrastEffect::enabledByDefault
    }, {
        QStringLiteral("coverswitch"),
        false,
        &createHelper<CoverSwitchEffect>,
        &CoverSwitchEffect::supported,
        nullptr
    }, {
        QStringLiteral("cube"),
        false,
        &createHelper<CubeEffect>,
        &CubeEffect::supported,
        nullptr
    }, {
        QStringLiteral("cubeslide"),
        false,
        &createHelper<CubeSlideEffect>,
        &CubeSlideEffect::supported,
        nullptr
    }, {
        QStringLiteral("dashboard"),
        true,
        &createHelper<DashboardEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("desktopgrid"),
        true,
        &createHelper<DesktopGridEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("diminactive"),
        false,
        &createHelper<DimInactiveEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("dimscreen"),
        false,
        &createHelper<DimScreenEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("fallapart"),
        false,
        &createHelper<FallApartEffect>,
        &FallApartEffect::supported,
        nullptr
    }, {
        QStringLiteral("flipswitch"),
        false,
        &createHelper<FlipSwitchEffect>,
        &FlipSwitchEffect::supported,
        nullptr
    }, {
        QStringLiteral("glide"),
        false,
        &createHelper<GlideEffect>,
        &GlideEffect::supported,
        nullptr
    }, {
        QStringLiteral("highlightwindow"),
        true,
        &createHelper<HighlightWindowEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("invert"),
        false,
        &createHelper<InvertEffect>,
        &InvertEffect::supported,
        nullptr
    }, {
        QStringLiteral("kscreen"),
        true,
        &createHelper<KscreenEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("logout"),
        true,
        &createHelper<LogoutEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("lookingglass"),
        false,
        &createHelper<LookingGlassEffect>,
        &LookingGlassEffect::supported,
        nullptr
    }, {
        QStringLiteral("magiclamp"),
        false,
        &createHelper<MagicLampEffect>,
        &MagicLampEffect::supported,
        nullptr
    }, {
        QStringLiteral("magnifier"),
        false,
        &createHelper<MagnifierEffect>,
        &MagnifierEffect::supported,
        nullptr
    }, {
        QStringLiteral("minimizeanimation"),
        true,
        &createHelper<MinimizeAnimationEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("mouseclick"),
        false,
        &createHelper<MouseClickEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("mousemark"),
        false,
        &createHelper<MouseMarkEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("presentwindows"),
        true,
        &createHelper<PresentWindowsEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("resize"),
        false,
        &createHelper<ResizeEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("screenedge"),
        true,
        &createHelper<ScreenEdgeEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("screenshot"),
        true,
        &createHelper<ScreenShotEffect>,
        &ScreenShotEffect::supported,
        nullptr
    }, {
        QStringLiteral("sheet"),
        false,
        &createHelper<SheetEffect>,
        &SheetEffect::supported,
        nullptr
    }, {
        QStringLiteral("showfps"),
        false,
        &createHelper<ShowFpsEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("showpaint"),
        false,
        &createHelper<ShowPaintEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("slide"),
        true,
        &createHelper<SlideEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("slideback"),
        false,
        &createHelper<SlideBackEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("slidingpopups"),
        true,
        &createHelper<SlidingPopupsEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("snaphelper"),
        false,
        &createHelper<SnapHelperEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("startupfeedback"),
        true,
        &createHelper<StartupFeedbackEffect>,
        &StartupFeedbackEffect::supported,
        nullptr
    }, {
        QStringLiteral("thumbnailaside"),
        false,
        &createHelper<ThumbnailAsideEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("trackmouse"),
        false,
        &createHelper<TrackMouseEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("windowgeometry"),
        false,
        &createHelper<WindowGeometry>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("wobblywindows"),
        false,
        &createHelper<WobblyWindowsEffect>,
        &WobblyWindowsEffect::supported,
        nullptr
    }, {
        QStringLiteral("zoom"),
        true,
        &createHelper<ZoomEffect>,
        nullptr,
        nullptr
    }
};

static inline int index(BuiltInEffect effect)
{
    return static_cast<int>(effect);
}

Effect *create(BuiltInEffect effect)
{
    const EffectData &effectData = s_effectData.at(index(effect));
    if (effectData.createFunction == nullptr) {
        return nullptr;
    }
    return effectData.createFunction();
}

bool available(const QString &name)
{
    auto it = std::find_if(s_effectData.begin(), s_effectData.end(),
        [name](const EffectData &data) {
            return data.name == name;
        }
    );
    return it != s_effectData.end();
}

bool supported(BuiltInEffect effect)
{
    if (effect == BuiltInEffect::Invalid) {
        return false;
    }
    const EffectData &effectData = s_effectData.at(index(effect));
    if (effectData.supportedFunction == nullptr) {
        return true;
    }
    return effectData.supportedFunction();
}

bool checkEnabledByDefault(BuiltInEffect effect)
{
    if (effect == BuiltInEffect::Invalid) {
        return false;
    }
    const EffectData &effectData = s_effectData.at(index(effect));
    if (effectData.enabledFunction == nullptr) {
        return true;
    }
    return effectData.enabledFunction();
}

bool enabledByDefault(BuiltInEffect effect)
{
    const EffectData &effectData = s_effectData.at(index(effect));
    return effectData.enabled;
}

QStringList availableEffectNames()
{
    QStringList result;
    for (const EffectData &data : s_effectData) {
        if (data.name.isEmpty()) {
            continue;
        }
        result << data.name;
    }
    return result;
}

QList< BuiltInEffect > availableEffects()
{
    QList<BuiltInEffect> result;
    for (int i = index(BuiltInEffect::Invalid) + 1; i <= index(BuiltInEffect::Zoom); ++i) {
        result << BuiltInEffect(i);
    }
    return result;
}

BuiltInEffect builtInForName(const QString &name)
{
    auto it = std::find_if(s_effectData.begin(), s_effectData.end(),
        [name](const EffectData &data) {
            return data.name == name;
        }
    );
    if (it == s_effectData.end()) {
        return BuiltInEffect::Invalid;
    }
    return BuiltInEffect(std::distance(s_effectData.begin(), it));
}

QString nameForEffect(BuiltInEffect effect)
{
    return s_effectData.at(index(effect)).name;
}

} // BuiltInEffects

} // namespace
