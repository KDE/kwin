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

static const QVector<EffectData> s_effectData = {
    {
        QString(),
        QString(),
        QString(),
        QString(),
        QString(),
        QUrl(),
        false,
        false,
        nullptr,
        nullptr,
        nullptr
    }, {
        QStringLiteral("blur"),
        i18nc("Name of a KWin Effect", "Blur"),
        i18nc("Comment describing the KWin Effect", "Blurs the background behind semi-transparent windows"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        true,
        false,
        &createHelper<BlurEffect>,
        &BlurEffect::supported,
        &BlurEffect::enabledByDefault
    }, {
        QStringLiteral("contrast"),
        i18nc("Name of a KWin Effect", "Background contrast"),
        i18nc("Comment describing the KWin Effect", "Improve contrast and readability behind semi-transparent windows"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        true,
        false,
        &createHelper<ContrastEffect>,
        &ContrastEffect::supported,
        &ContrastEffect::enabledByDefault
    }, {
        QStringLiteral("coverswitch"),
        i18nc("Name of a KWin Effect", "Cover Switch"),
        i18nc("Comment describing the KWin Effect", "Display a Cover Flow effect for the alt+tab window switcher"),
        QStringLiteral("Window Management"),
        QString(),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/cover_switch.mp4")),
        false,
        true,
        &createHelper<CoverSwitchEffect>,
        &CoverSwitchEffect::supported,
        nullptr
    }, {
        QStringLiteral("cube"),
        i18nc("Name of a KWin Effect", "Desktop Cube"),
        i18nc("Comment describing the KWin Effect", "Display each virtual desktop on a side of a cube"),
        QStringLiteral("Window Management"),
        QString(),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/desktop_cube.ogv")),
        false,
        false,
        &createHelper<CubeEffect>,
        &CubeEffect::supported,
        nullptr
    }, {
        QStringLiteral("cubeslide"),
        i18nc("Name of a KWin Effect", "Desktop Cube Animation"),
        i18nc("Comment describing the KWin Effect", "Animate desktop switching with a cube"),
        QStringLiteral("Virtual Desktop Switching Animation"),
        QStringLiteral("desktop-animations"),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/desktop_cube_animation.ogv")),
        false,
        false,
        &createHelper<CubeSlideEffect>,
        &CubeSlideEffect::supported,
        nullptr
    }, {
        QStringLiteral("dashboard"),
        i18nc("Name of a KWin Effect", "Dashboard"),
        i18nc("Comment describing the KWin Effect", "Desaturate the desktop when displaying the Plasma dashboard"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        true,
        true,
        &createHelper<DashboardEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("desktopgrid"),
        i18nc("Name of a KWin Effect", "Desktop Grid"),
        i18nc("Comment describing the KWin Effect", "Zoom out so all desktops are displayed side-by-side in a grid"),
        QStringLiteral("Window Management"),
        QString(),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/desktop_grid.mp4")),
        true,
        false,
        &createHelper<DesktopGridEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("diminactive"),
        i18nc("Name of a KWin Effect", "Dim Inactive"),
        i18nc("Comment describing the KWin Effect", "Darken inactive windows"),
        QStringLiteral("Focus"),
        QString(),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/dim_inactive.mp4")),
        false,
        false,
        &createHelper<DimInactiveEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("dimscreen"),
        i18nc("Name of a KWin Effect", "Dim Screen for Administrator Mode"),
        i18nc("Comment describing the KWin Effect", "Darkens the entire screen when requesting root privileges"),
        QStringLiteral("Focus"),
        QString(),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/dim_administration.mp4")),
        false,
        false,
        &createHelper<DimScreenEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("fallapart"),
        i18nc("Name of a KWin Effect", "Fall Apart"),
        i18nc("Comment describing the KWin Effect", "Closed windows fall into pieces"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        false,
        false,
        &createHelper<FallApartEffect>,
        &FallApartEffect::supported,
        nullptr
    }, {
        QStringLiteral("flipswitch"),
        i18nc("Name of a KWin Effect", "Flip Switch"),
        i18nc("Comment describing the KWin Effect", "Flip through windows that are in a stack for the alt+tab window switcher"),
        QStringLiteral("Window Management"),
        QString(),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/flip_switch.mp4")),
        false,
        false,
        &createHelper<FlipSwitchEffect>,
        &FlipSwitchEffect::supported,
        nullptr
    }, {
        QStringLiteral("glide"),
        i18nc("Name of a KWin Effect", "Glide"),
        i18nc("Comment describing the KWin Effect", "Windows Glide Effect as they are open and closed"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        false,
        false,
        &createHelper<GlideEffect>,
        &GlideEffect::supported,
        nullptr
    }, {
        QStringLiteral("highlightwindow"),
        i18nc("Name of a KWin Effect", "Highlight Window"),
        i18nc("Comment describing the KWin Effect", "Highlight the appropriate window when hovering over taskbar entries"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        true,
        true,
        &createHelper<HighlightWindowEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("invert"),
        i18nc("Name of a KWin Effect", "Invert"),
        i18nc("Comment describing the KWin Effect", "Inverts the color of the desktop and windows"),
        QStringLiteral("Accessibility"),
        QString(),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/invert.mp4")),
        false,
        false,
        &createHelper<InvertEffect>,
        &InvertEffect::supported,
        nullptr
    }, {
        QStringLiteral("kscreen"),
        i18nc("Name of a KWin Effect", "Kscreen"),
        i18nc("Comment describing the KWin Effect", "Helper Effect for KScreen"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        true,
        true,
        &createHelper<KscreenEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("logout"),
        i18nc("Name of a KWin Effect", "Logout"),
        i18nc("Comment describing the KWin Effect", "Desaturate the desktop when displaying the logout dialog"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        true,
        false,
        &createHelper<LogoutEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("lookingglass"),
        i18nc("Name of a KWin Effect", "Looking Glass"),
        i18nc("Comment describing the KWin Effect", "A screen magnifier that looks like a fisheye lens"),
        QStringLiteral("Accessibility"),
        QStringLiteral("magnifiers"),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/looking_glass.ogv")),
        false,
        false,
        &createHelper<LookingGlassEffect>,
        &LookingGlassEffect::supported,
        nullptr
    }, {
        QStringLiteral("magiclamp"),
        i18nc("Name of a KWin Effect", "Magic Lamp"),
        i18nc("Comment describing the KWin Effect", "Simulate a magic lamp when minimizing windows"),
        QStringLiteral("Appearance"),
        QStringLiteral("minimize"),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/magic_lamp.ogv")),
        false,
        false,
        &createHelper<MagicLampEffect>,
        &MagicLampEffect::supported,
        nullptr
    }, {
        QStringLiteral("magnifier"),
        i18nc("Name of a KWin Effect", "Magnifier"),
        i18nc("Comment describing the KWin Effect", "Magnify the section of the screen that is near the mouse cursor"),
        QStringLiteral("Accessibility"),
        QStringLiteral("magnifiers"),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/magnifier.ogv")),
        false,
        false,
        &createHelper<MagnifierEffect>,
        &MagnifierEffect::supported,
        nullptr
    }, {
        QStringLiteral("minimizeanimation"),
        i18nc("Name of a KWin Effect", "Minimize Animation"),
        i18nc("Comment describing the KWin Effect", "Animate the minimizing of windows"),
        QStringLiteral("Appearance"),
        QStringLiteral("minimize"),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/minimize.ogv")),
        true,
        false,
        &createHelper<MinimizeAnimationEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("mouseclick"),
        i18nc("Name of a KWin Effect", "Mouse Click Animation"),
        i18nc("Comment describing the KWin Effect", "Creates an animation whenever a mouse button is clicked. This is useful for screenrecordings/presentations"),
        QStringLiteral("Accessibility"),
        QString(),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/mouse_click.mp4")),
        false,
        false,
        &createHelper<MouseClickEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("mousemark"),
        i18nc("Name of a KWin Effect", "Mouse Mark"),
        i18nc("Comment describing the KWin Effect", "Allows you to draw lines on the desktop"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        false,
        false,
        &createHelper<MouseMarkEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("presentwindows"),
        i18nc("Name of a KWin Effect", "Present Windows"),
        i18nc("Comment describing the KWin Effect", "Zoom out until all opened windows can be displayed side-by-side"),
        QStringLiteral("Window Management"),
        QString(),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/present_windows.mp4")),
        true,
        false,
        &createHelper<PresentWindowsEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("resize"),
        i18nc("Name of a KWin Effect", "Resize Window"),
        i18nc("Comment describing the KWin Effect", "Resizes windows with a fast texture scale instead of updating contents"),
        QStringLiteral("Window Management"),
        QString(),
        QUrl(),
        false,
        false,
        &createHelper<ResizeEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("screenedge"),
        i18nc("Name of a KWin Effect", "Screen Edge"),
        i18nc("Comment describing the KWin Effect", "Highlights a screen edge when approaching"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        true,
        false,
        &createHelper<ScreenEdgeEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("screenshot"),
        i18nc("Name of a KWin Effect", "Screenshot"),
        i18nc("Comment describing the KWin Effect", "Helper effect for KSnapshot"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        true,
        true,
        &createHelper<ScreenShotEffect>,
        &ScreenShotEffect::supported,
        nullptr
    }, {
        QStringLiteral("sheet"),
        i18nc("Name of a KWin Effect", "Sheet"),
        i18nc("Comment describing the KWin Effect", "Make modal dialogs smoothly fly in and out when they are shown or hidden"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        false,
        false,
        &createHelper<SheetEffect>,
        &SheetEffect::supported,
        nullptr
    }, {
        QStringLiteral("showfps"),
        i18nc("Name of a KWin Effect", "Show FPS"),
        i18nc("Comment describing the KWin Effect", "Display KWin's performance in the corner of the screen"),
        QStringLiteral("Tools"),
        QString(),
        QUrl(),
        false,
        false,
        &createHelper<ShowFpsEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("showpaint"),
        i18nc("Name of a KWin Effect", "Show Paint"),
        i18nc("Comment describing the KWin Effect", "Highlight areas of the desktop that have been recently updated"),
        QStringLiteral("Tools"),
        QString(),
        QUrl(),
        false,
        false,
        &createHelper<ShowPaintEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("slide"),
        i18nc("Name of a KWin Effect", "Slide"),
        i18nc("Comment describing the KWin Effect", "Slide windows across the screen when switching virtual desktops"),
        QStringLiteral("Virtual Desktop Switching Animation"),
        QStringLiteral("desktop-animations"),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/slide.ogv")),
        true,
        false,
        &createHelper<SlideEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("slideback"),
        i18nc("Name of a KWin Effect", "Slide Back"),
        i18nc("Comment describing the KWin Effect", "Slide back windows when another window is raised"),
        QStringLiteral("Focus"),
        QString(),
        QUrl(),
        false,
        false,
        &createHelper<SlideBackEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("slidingpopups"),
        i18nc("Name of a KWin Effect", "Sliding popups"),
        i18nc("Comment describing the KWin Effect", "Sliding animation for Plasma popups"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/sliding_popups.mp4")),
        true,
        false,
        &createHelper<SlidingPopupsEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("snaphelper"),
        i18nc("Name of a KWin Effect", "Snap Helper"),
        i18nc("Comment describing the KWin Effect", "Help you locate the center of the screen when moving a window"),
        QStringLiteral("Accessibility"),
        QString(),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/snap_helper.mp4")),
        false,
        false,
        &createHelper<SnapHelperEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("startupfeedback"),
        i18nc("Name of a KWin Effect", "Startup Feedback"),
        i18nc("Comment describing the KWin Effect", "Helper effect for startup feedback"),
        QStringLiteral("Candy"),
        QString(),
        QUrl(),
        true,
        true,
        &createHelper<StartupFeedbackEffect>,
        &StartupFeedbackEffect::supported,
        nullptr
    }, {
        QStringLiteral("thumbnailaside"),
        i18nc("Name of a KWin Effect", "Thumbnail Aside"),
        i18nc("Comment describing the KWin Effect", "Display window thumbnails on the edge of the screen"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        false,
        false,
        &createHelper<ThumbnailAsideEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("trackmouse"),
        i18nc("Name of a KWin Effect", "Track Mouse"),
        i18nc("Comment describing the KWin Effect", "Display a mouse cursor locating effect when activated"),
        QStringLiteral("Accessibility"),
        QString(),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/track_mouse.mp4")),
        false,
        false,
        &createHelper<TrackMouseEffect>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("windowgeometry"),
        i18nc("Name of a KWin Effect", "Window Geometry"),
        i18nc("Comment describing the KWin Effect", "Display window geometries on move/resize"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        false,
        true,
        &createHelper<WindowGeometry>,
        nullptr,
        nullptr
    }, {
        QStringLiteral("wobblywindows"),
        i18nc("Name of a KWin Effect", "Wobbly Windows"),
        i18nc("Comment describing the KWin Effect", "Deform windows while they are moving"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/wobbly_windows.ogv")),
        false,
        false,
        &createHelper<WobblyWindowsEffect>,
        &WobblyWindowsEffect::supported,
        nullptr
    }, {
        QStringLiteral("zoom"),
        i18nc("Name of a KWin Effect", "Zoom"),
        i18nc("Comment describing the KWin Effect", "Magnify the entire desktop"),
        QStringLiteral("Accessibility"),
        QStringLiteral("magnifiers"),
        QUrl(QStringLiteral("http://files.kde.org/plasma/kwin/effect-videos/zoom.ogv")),
        true,
        false,
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

const EffectData &effectData(BuiltInEffect effect)
{
    return s_effectData.at(index(effect));
}

} // BuiltInEffects

} // namespace
