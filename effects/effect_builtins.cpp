/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "effect_builtins.h"
#ifdef EFFECT_BUILTINS
// common effects
#include "backgroundcontrast/contrast.h"
#include "blur/blur.h"
#include "colorpicker/colorpicker.h"
#include "kscreen/kscreen.h"
#include "presentwindows/presentwindows.h"
#include "screenedge/screenedgeeffect.h"
#include "screenshot/screenshot.h"
#include "slidingpopups/slidingpopups.h"
// Common effects only relevant to desktop
#include "desktopgrid/desktopgrid.h"
#include "diminactive/diminactive.h"
#include "fallapart/fallapart.h"
#include "highlightwindow/highlightwindow.h"
#include "magiclamp/magiclamp.h"
#include "resize/resize.h"
#include "showfps/showfps.h"
#include "showpaint/showpaint.h"
#include "slide/slide.h"
#include "slideback/slideback.h"
#include "thumbnailaside/thumbnailaside.h"
#include "touchpoints/touchpoints.h"
#include "windowgeometry/windowgeometry.h"
#include "zoom/zoom.h"
// OpenGL-specific effects for desktop
#include "coverswitch/coverswitch.h"
#include "cube/cube.h"
#include "cubeslide/cubeslide.h"
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
#endif

#include <KLocalizedString>
#include <kwineffects.h>

#ifndef EFFECT_BUILTINS
#define EFFECT_FALLBACK nullptr, nullptr, nullptr
#else
#define EFFECT_FALLBACK
#endif

namespace KWin
{

namespace BuiltInEffects
{

template <class T>
inline Effect *createHelper()
{
    return new T();
}

static const QVector<EffectData> &effectData()
{
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
        i18ndc("kwin_effects", "Name of a KWin Effect", "Blur"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Blurs the background behind semi-transparent windows"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        true,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<BlurEffect>,
        &BlurEffect::supported,
        &BlurEffect::enabledByDefault
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("colorpicker"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Color Picker"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Supports picking a color"),
        QStringLiteral("Accessibility"),
        QString(),
        QUrl(),
        true,
        true,
#ifdef EFFECT_BUILTINS
        &createHelper<ColorPickerEffect>,
        &ColorPickerEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("contrast"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Background contrast"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Improve contrast and readability behind semi-transparent windows"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        true,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<ContrastEffect>,
        &ContrastEffect::supported,
        &ContrastEffect::enabledByDefault
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("coverswitch"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Cover Switch"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Display a Cover Flow effect for the alt+tab window switcher"),
        QStringLiteral("Window Management"),
        QString(),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/cover_switch.mp4")),
        false,
        true,
#ifdef EFFECT_BUILTINS
        &createHelper<CoverSwitchEffect>,
        &CoverSwitchEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("cube"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Desktop Cube"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Display each virtual desktop on a side of a cube"),
        QStringLiteral("Window Management"),
        QString(),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/desktop_cube.ogv")),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<CubeEffect>,
        &CubeEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("cubeslide"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Desktop Cube Animation"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Animate desktop switching with a cube"),
        QStringLiteral("Virtual Desktop Switching Animation"),
        QStringLiteral("desktop-animations"),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/desktop_cube_animation.ogv")),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<CubeSlideEffect>,
        &CubeSlideEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("desktopgrid"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Desktop Grid"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Zoom out so all desktops are displayed side-by-side in a grid"),
        QStringLiteral("Window Management"),
        QString(),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/desktop_grid.mp4")),
        true,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<DesktopGridEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("diminactive"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Dim Inactive"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Darken inactive windows"),
        QStringLiteral("Focus"),
        QString(),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/dim_inactive.mp4")),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<DimInactiveEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("fallapart"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Fall Apart"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Closed windows fall into pieces"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<FallApartEffect>,
        &FallApartEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("flipswitch"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Flip Switch"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Flip through windows that are in a stack for the alt+tab window switcher"),
        QStringLiteral("Window Management"),
        QString(),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/flip_switch.mp4")),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<FlipSwitchEffect>,
        &FlipSwitchEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("glide"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Glide"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Glide windows as they appear or disappear"),
        QStringLiteral("Window Open/Close Animation"),
        QStringLiteral("toplevel-open-close-animation"),
        QUrl(),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<GlideEffect>,
        &GlideEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("highlightwindow"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Highlight Window"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Highlight the appropriate window when hovering over taskbar entries"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        true,
        true,
#ifdef EFFECT_BUILTINS
        &createHelper<HighlightWindowEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("invert"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Invert"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Inverts the color of the desktop and windows"),
        QStringLiteral("Accessibility"),
        QString(),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/invert.mp4")),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<InvertEffect>,
        &InvertEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("kscreen"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Kscreen"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Helper Effect for KScreen"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        true,
        true,
#ifdef EFFECT_BUILTINS
        &createHelper<KscreenEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("lookingglass"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Looking Glass"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "A screen magnifier that looks like a fisheye lens"),
        QStringLiteral("Accessibility"),
        QStringLiteral("magnifiers"),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/looking_glass.ogv")),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<LookingGlassEffect>,
        &LookingGlassEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("magiclamp"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Magic Lamp"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Simulate a magic lamp when minimizing windows"),
        QStringLiteral("Appearance"),
        QStringLiteral("minimize"),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/magic_lamp.ogv")),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<MagicLampEffect>,
        &MagicLampEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("magnifier"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Magnifier"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Magnify the section of the screen that is near the mouse cursor"),
        QStringLiteral("Accessibility"),
        QStringLiteral("magnifiers"),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/magnifier.ogv")),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<MagnifierEffect>,
        &MagnifierEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("mouseclick"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Mouse Click Animation"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Creates an animation whenever a mouse button is clicked. This is useful for screenrecordings/presentations"),
        QStringLiteral("Accessibility"),
        QString(),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/mouse_click.mp4")),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<MouseClickEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("mousemark"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Mouse Mark"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Allows you to draw lines on the desktop"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<MouseMarkEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("presentwindows"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Present Windows"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Zoom out until all opened windows can be displayed side-by-side"),
        QStringLiteral("Window Management"),
        QString(),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/present_windows.mp4")),
        true,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<PresentWindowsEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("resize"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Resize Window"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Resizes windows with a fast texture scale instead of updating contents"),
        QStringLiteral("Window Management"),
        QString(),
        QUrl(),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<ResizeEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("screenedge"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Screen Edge"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Highlights a screen edge when approaching"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        true,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<ScreenEdgeEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("screenshot"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Screenshot"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Helper effect for screenshot tools"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        true,
        true,
#ifdef EFFECT_BUILTINS
        &createHelper<ScreenShotEffect>,
        &ScreenShotEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("sheet"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Sheet"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Make modal dialogs smoothly fly in and out when they are shown or hidden"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<SheetEffect>,
        &SheetEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("showfps"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Show FPS"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Display KWin's performance in the corner of the screen"),
        QStringLiteral("Tools"),
        QString(),
        QUrl(),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<ShowFpsEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("showpaint"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Show Paint"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Highlight areas of the desktop that have been recently updated"),
        QStringLiteral("Tools"),
        QString(),
        QUrl(),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<ShowPaintEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("slide"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Slide"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Slide desktops when switching virtual desktops"),
        QStringLiteral("Virtual Desktop Switching Animation"),
        QStringLiteral("desktop-animations"),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/slide.ogv")),
        true,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<SlideEffect>,
        &SlideEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("slideback"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Slide Back"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Slide back windows when another window is raised"),
        QStringLiteral("Focus"),
        QString(),
        QUrl(),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<SlideBackEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("slidingpopups"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Sliding popups"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Sliding animation for Plasma popups"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/sliding_popups.mp4")),
        true,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<SlidingPopupsEffect>,
        &SlidingPopupsEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("snaphelper"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Snap Helper"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Help you locate the center of the screen when moving a window"),
        QStringLiteral("Accessibility"),
        QString(),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/snap_helper.mp4")),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<SnapHelperEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("startupfeedback"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Startup Feedback"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Helper effect for startup feedback"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        true,
        true,
#ifdef EFFECT_BUILTINS
        &createHelper<StartupFeedbackEffect>,
        &StartupFeedbackEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("thumbnailaside"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Thumbnail Aside"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Display window thumbnails on the edge of the screen"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<ThumbnailAsideEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("touchpoints"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Touch Points"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Visualize touch points"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<TouchPointsEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("trackmouse"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Track Mouse"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Display a mouse cursor locating effect when activated"),
        QStringLiteral("Accessibility"),
        QString(),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/track_mouse.mp4")),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<TrackMouseEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("windowgeometry"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Window Geometry"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Display window geometries on move/resize"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(),
        false,
        true,
#ifdef EFFECT_BUILTINS
        &createHelper<WindowGeometry>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("wobblywindows"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Wobbly Windows"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Deform windows while they are moving"),
        QStringLiteral("Appearance"),
        QString(),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/wobbly_windows.ogv")),
        false,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<WobblyWindowsEffect>,
        &WobblyWindowsEffect::supported,
        nullptr
#endif
EFFECT_FALLBACK
    }, {
        QStringLiteral("zoom"),
        i18ndc("kwin_effects", "Name of a KWin Effect", "Zoom"),
        i18ndc("kwin_effects", "Comment describing the KWin Effect", "Magnify the entire desktop"),
        QStringLiteral("Accessibility"),
        QStringLiteral("magnifiers"),
        QUrl(QStringLiteral("https://files.kde.org/plasma/kwin/effect-videos/zoom.ogv")),
        true,
        false,
#ifdef EFFECT_BUILTINS
        &createHelper<ZoomEffect>,
        nullptr,
        nullptr
#endif
EFFECT_FALLBACK
    }
    };
    return s_effectData;
}

static inline int index(BuiltInEffect effect)
{
    return static_cast<int>(effect);
}

Effect *create(BuiltInEffect effect)
{
    const EffectData &data = effectData(effect);
    if (data.createFunction == nullptr) {
        return nullptr;
    }
    return data.createFunction();
}

bool available(const QString &name)
{
    auto it = std::find_if(effectData().begin(), effectData().end(),
        [name](const EffectData &data) {
            return data.name == name;
        }
    );
    return it != effectData().end();
}

bool supported(BuiltInEffect effect)
{
    if (effect == BuiltInEffect::Invalid) {
        return false;
    }
    const EffectData &data = effectData(effect);
    if (data.supportedFunction == nullptr) {
        return true;
    }
    return data.supportedFunction();
}

bool checkEnabledByDefault(BuiltInEffect effect)
{
    if (effect == BuiltInEffect::Invalid) {
        return false;
    }
    const EffectData &data = effectData(effect);
    if (data.enabledFunction == nullptr) {
        return true;
    }
    return data.enabledFunction();
}

bool enabledByDefault(BuiltInEffect effect)
{
    return effectData(effect).enabled;
}

QStringList availableEffectNames()
{
    QStringList result;
    for (const EffectData &data : effectData()) {
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
    auto it = std::find_if(effectData().begin(), effectData().end(),
        [name](const EffectData &data) {
            return data.name == name;
        }
    );
    if (it == effectData().end()) {
        return BuiltInEffect::Invalid;
    }
    return BuiltInEffect(std::distance(effectData().begin(), it));
}

QString nameForEffect(BuiltInEffect effect)
{
    return effectData(effect).name;
}

const EffectData &effectData(BuiltInEffect effect)
{
    return effectData().at(index(effect));
}

} // BuiltInEffects

} // namespace
