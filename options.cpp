/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

#include "options.h"
#include "config-kwin.h"

#ifndef KCMRULES

#include <QPalette>
#include <QPixmap>
#include <QProcess>
#include <kapplication.h>
#include <kconfig.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <klocale.h>

#include <QDesktopWidget>

#include "client.h"
#include "compositingprefs.h"
#include <kwinglplatform.h>

#include <kephal/screens.h>

#include <X11/extensions/Xrandr.h>

#endif

namespace KWin
{

#ifndef KCMRULES

int currentRefreshRate()
{
    int rate = -1;
    if (options->refreshRate > 0)   // use manually configured refresh rate
        rate = options->refreshRate;
#ifndef KWIN_HAVE_OPENGLES
    else if (GLPlatform::instance()->driver() == Driver_NVidia) {
        QProcess nvidia_settings;
        nvidia_settings.start("nvidia-settings", QStringList() << "-t" << "-q" << "RefreshRate", QIODevice::ReadOnly);
        nvidia_settings.waitForFinished();
        if (nvidia_settings.exitStatus() == QProcess::NormalExit) {
            QString reply = QString::fromLocal8Bit(nvidia_settings.readAllStandardOutput());
            bool ok;
            const float frate = reply.split(' ').first().toFloat(&ok);
            if (!ok)
                rate = -1;
            else
                rate = qRound(frate);
        }
    }
#endif
    else if (Extensions::randrAvailable()) {
        XRRScreenConfiguration *config = XRRGetScreenInfo(display(), rootWindow());
        rate = XRRConfigCurrentRate(config);
        XRRFreeScreenConfigInfo(config);
    }

    // 0Hz or less is invalid, so we fallback to a default rate
    if (rate <= 0)
        rate = 50;
    // QTimer gives us 1msec (1000Hz) at best, so we ignore anything higher;
    // however, additional throttling prevents very high rates from taking place anyway
    else if (rate > 1000)
        rate = 1000;
    kDebug(1212) << "Refresh rate " << rate << "Hz";
    return rate;
}

Options::Options()
    : electric_borders(0)
    , electric_border_delay(0)
{
    compositingInitialized = false;
    updateSettings();
}

Options::~Options()
{
}

unsigned long Options::updateSettings()
{
    KSharedConfig::Ptr _config = KGlobal::config();
    unsigned long changed = 0;
    changed |= KDecorationOptions::updateSettings(_config.data());   // read decoration settings

    KConfigGroup config(_config, "Windows");
    show_geometry_tip = config.readEntry("GeometryTip", false);

    QString val;

    val = config.readEntry("FocusPolicy", "ClickToFocus");
    focusPolicy = ClickToFocus; // what a default :-)
    if (val == "FocusFollowsMouse")
        focusPolicy = FocusFollowsMouse;
    else if (val == "FocusUnderMouse")
        focusPolicy = FocusUnderMouse;
    else if (val == "FocusStrictlyUnderMouse")
        focusPolicy = FocusStrictlyUnderMouse;

    separateScreenFocus = config.readEntry("SeparateScreenFocus", false);
    activeMouseScreen = config.readEntry("ActiveMouseScreen", focusPolicy != ClickToFocus);

    rollOverDesktops = config.readEntry("RollOverDesktops", true);

    legacyFullscreenSupport = config.readEntry("LegacyFullscreenSupport", false);

//    focusStealingPreventionLevel = config.readEntry( "FocusStealingPreventionLevel", 2 );
    // TODO use low level for now
    focusStealingPreventionLevel = config.readEntry("FocusStealingPreventionLevel", 1);
    focusStealingPreventionLevel = qMax(0, qMin(4, focusStealingPreventionLevel));
    if (!focusPolicyIsReasonable())  // #48786, comments #7 and later
        focusStealingPreventionLevel = 0;

    xineramaEnabled = config.readEntry("XineramaEnabled", true);
    xineramaPlacementEnabled = config.readEntry("XineramaPlacementEnabled", true);
    xineramaMovementEnabled = config.readEntry("XineramaMovementEnabled", true);
    xineramaMaximizeEnabled = config.readEntry("XineramaMaximizeEnabled", true);
    xineramaFullscreenEnabled = config.readEntry("XineramaFullscreenEnabled", true);

#ifdef KWIN_BUILD_DECORATIONS
    placement = Placement::policyFromString(config.readEntry("Placement"), true);
#else
    placement = Placement::Maximizing;
#endif
    xineramaPlacementScreen = qBound(-1, config.readEntry("XineramaPlacementScreen", -1),
                                     Kephal::ScreenUtils::numScreens() - 1);

    if (focusPolicy == ClickToFocus) {
        autoRaise = false;
        autoRaiseInterval = 0;
        delayFocus = false;
        delayFocusInterval = 0;
    } else {
        autoRaise = config.readEntry("AutoRaise", false);
        autoRaiseInterval = config.readEntry("AutoRaiseInterval", 0);
        delayFocus = config.readEntry("DelayFocus", false);
        delayFocusInterval = config.readEntry("DelayFocusInterval", 0);
    }

    shadeHover = config.readEntry("ShadeHover", false);
    shadeHoverInterval = config.readEntry("ShadeHoverInterval", 250);

    tilingOn = config.readEntry("TilingOn", false);
    tilingLayout = static_cast<TilingLayoutFactory::Layouts>(config.readEntry("TilingDefaultLayout", 0));
    tilingRaisePolicy = config.readEntry("TilingRaisePolicy", 0);

    // important: autoRaise implies ClickRaise
    clickRaise = autoRaise || config.readEntry("ClickRaise", true);

    borderSnapZone = config.readEntry("BorderSnapZone", 10);
    windowSnapZone = config.readEntry("WindowSnapZone", 10);
    centerSnapZone = config.readEntry("CenterSnapZone", 0);
    snapOnlyWhenOverlapping = config.readEntry("SnapOnlyWhenOverlapping", false);

    // Electric borders
    KConfigGroup borderConfig(_config, "ElectricBorders");
    electric_border_top = electricBorderAction(borderConfig.readEntry("Top", "None"));
    electric_border_top_right = electricBorderAction(borderConfig.readEntry("TopRight", "None"));
    electric_border_right = electricBorderAction(borderConfig.readEntry("Right", "None"));
    electric_border_bottom_right = electricBorderAction(borderConfig.readEntry("BottomRight", "None"));
    electric_border_bottom = electricBorderAction(borderConfig.readEntry("Bottom", "None"));
    electric_border_bottom_left = electricBorderAction(borderConfig.readEntry("BottomLeft", "None"));
    electric_border_left = electricBorderAction(borderConfig.readEntry("Left", "None"));
    electric_border_top_left = electricBorderAction(borderConfig.readEntry("TopLeft", "None"));
    electric_borders = config.readEntry("ElectricBorders", 0);
    electric_border_delay = config.readEntry("ElectricBorderDelay", 150);
    electric_border_cooldown = config.readEntry("ElectricBorderCooldown", 350);
    electric_border_pushback_pixels = config.readEntry("ElectricBorderPushbackPixels", 1);
    electric_border_maximize = config.readEntry("ElectricBorderMaximize", true);
    electric_border_tiling = config.readEntry("ElectricBorderTiling", true);

    OpTitlebarDblClick = windowOperation(config.readEntry("TitlebarDoubleClickCommand", "Maximize"), true);
    setOpMaxButtonLeftClick(windowOperation(config.readEntry("MaximizeButtonLeftClickCommand", "Maximize"), true));
    setOpMaxButtonMiddleClick(windowOperation(config.readEntry("MaximizeButtonMiddleClickCommand", "Maximize (vertical only)"), true));
    setOpMaxButtonRightClick(windowOperation(config.readEntry("MaximizeButtonRightClickCommand", "Maximize (horizontal only)"), true));

    ignorePositionClasses = config.readEntry("IgnorePositionClasses", QStringList());
    ignoreFocusStealingClasses = config.readEntry("IgnoreFocusStealingClasses", QStringList());
    // Qt3.2 and older had resource class all lowercase, but Qt3.3 has it capitalized
    // therefore Client::resourceClass() forces lowercase, force here lowercase as well
    for (QStringList::Iterator it = ignorePositionClasses.begin();
            it != ignorePositionClasses.end();
            ++it)
        (*it) = (*it).toLower();
    for (QStringList::Iterator it = ignoreFocusStealingClasses.begin();
            it != ignoreFocusStealingClasses.end();
            ++it)
        (*it) = (*it).toLower();

    killPingTimeout = config.readEntry("KillPingTimeout", 5000);
    hideUtilityWindowsForInactive = config.readEntry("HideUtilityWindowsForInactive", true);
    inactiveTabsSkipTaskbar = config.readEntry("InactiveTabsSkipTaskbar", false);
    autogroupSimilarWindows = config.readEntry("AutogroupSimilarWindows", false);
    autogroupInForeground = config.readEntry("AutogroupInForeground", true);
    showDesktopIsMinimizeAll = config.readEntry("ShowDesktopIsMinimizeAll", false);

    borderless_maximized_windows = config.readEntry("BorderlessMaximizedWindows", false);

    // Mouse bindings
    config = KConfigGroup(_config, "MouseBindings");
    CmdActiveTitlebar1 = mouseCommand(config.readEntry("CommandActiveTitlebar1", "Raise"), true);
    CmdActiveTitlebar2 = mouseCommand(config.readEntry("CommandActiveTitlebar2", "Start Window Tab Drag"), true);
    CmdActiveTitlebar3 = mouseCommand(config.readEntry("CommandActiveTitlebar3", "Operations menu"), true);
    CmdInactiveTitlebar1 = mouseCommand(config.readEntry("CommandInactiveTitlebar1", "Activate and raise"), true);
    CmdInactiveTitlebar2 = mouseCommand(config.readEntry("CommandInactiveTitlebar2", "Start Window Tab Drag"), true);
    CmdInactiveTitlebar3 = mouseCommand(config.readEntry("CommandInactiveTitlebar3", "Operations menu"), true);
    CmdTitlebarWheel = mouseWheelCommand(config.readEntry("CommandTitlebarWheel", "Switch to Window Tab to the Left/Right"));
    CmdWindow1 = mouseCommand(config.readEntry("CommandWindow1", "Activate, raise and pass click"), false);
    CmdWindow2 = mouseCommand(config.readEntry("CommandWindow2", "Activate and pass click"), false);
    CmdWindow3 = mouseCommand(config.readEntry("CommandWindow3", "Activate and pass click"), false);
    CmdWindowWheel = mouseCommand(config.readEntry("CommandWindowWheel", "Scroll"), false);
    CmdAllModKey = (config.readEntry("CommandAllKey", "Alt") == "Meta") ? Qt::Key_Meta : Qt::Key_Alt;
    CmdAll1 = mouseCommand(config.readEntry("CommandAll1", "Move"), false);
    CmdAll2 = mouseCommand(config.readEntry("CommandAll2", "Toggle raise and lower"), false);
    CmdAll3 = mouseCommand(config.readEntry("CommandAll3", "Resize"), false);
    CmdAllWheel = mouseWheelCommand(config.readEntry("CommandAllWheel", "Nothing"));

    config = KConfigGroup(_config, "Compositing");
    maxFpsInterval = qRound(1000.0 / config.readEntry("MaxFPS", 60));
    refreshRate = config.readEntry("RefreshRate", 0);

    // Read button tooltip animation effect from kdeglobals
    // Since we want to allow users to enable window decoration tooltips
    // and not kstyle tooltips and vise-versa, we don't read the
    // "EffectNoTooltip" setting from kdeglobals.


//    QToolTip::setGloballyEnabled( d->show_tooltips );
// KDE4 this probably needs to be done manually in clients

    // Driver-specific config detection
    compositingInitialized = false;
    reloadCompositingSettings();

    return changed;
}

void Options::reloadCompositingSettings(bool force)
{
    KSharedConfig::Ptr _config = KGlobal::config();
    KConfigGroup config(_config, "Compositing");

    QString compositingBackend = config.readEntry("Backend", "OpenGL");
    if (compositingBackend == "XRender")
        compositingMode = XRenderCompositing;
    else
        compositingMode = OpenGLCompositing;

    useCompositing = false;
    if (const char *c = getenv("KWIN_COMPOSE")) {
        switch(c[0]) {
        case 'O':
            kDebug(1212) << "Compositing forced to OpenGL mode by environment variable";
            compositingMode = OpenGLCompositing;
            useCompositing = true;
            break;
        case 'X':
            kDebug(1212) << "Compositing forced to XRender mode by environment variable";
            compositingMode = XRenderCompositing;
            useCompositing = true;
            break;
        case 'N':
            if (getenv("KDE_FAILSAFE"))
                kDebug(1212) << "Compositing disabled forcefully by KDE failsafe mode";
            else
                kDebug(1212) << "Compositing disabled forcefully by environment variable";
            compositingMode = NoCompositing;
            break;
        default:
            kDebug(1212) << "Unknown KWIN_COMPOSE mode set, ignoring";
            break;
        }
    }

    if (compositingMode == NoCompositing)
        return; // do not even detect compositing preferences if explicitly disabled

    // it's either enforced by env or by initial resume from "suspend" or we check the settings
    useCompositing = useCompositing || force || config.readEntry("Enabled", true);

    if (!useCompositing)
        return; // not enforced or necessary and not "enabled" by setting

    // from now on we've an initial setup and don't have to reload settigns on compositing activation
    // see Workspace::setupCompositing(), composite.cpp
    compositingInitialized = true;

    // Compositing settings
    CompositingPrefs prefs;
    prefs.detect();

    useCompositing = config.readEntry("Enabled" , prefs.recommendCompositing());
    glDirect = prefs.enableDirectRendering();
    glVSync = config.readEntry("GLVSync", prefs.enableVSync());
    glSmoothScale = qBound(-1, config.readEntry("GLTextureFilter", 2), 2);
    glStrictBinding = config.readEntry("GLStrictBinding", prefs.strictBinding());

    xrenderSmoothScale = config.readEntry("XRenderSmoothScale", false);

    hiddenPreviews = HiddenPreviewsShown;
    // 4 - off, 5 - shown, 6 - always, other are old values
    int hps = config.readEntry("HiddenPreviews", 5);
    if (hps == 4)
        hiddenPreviews = HiddenPreviewsNever;
    else if (hps == 5)
        hiddenPreviews = HiddenPreviewsShown;
    else if (hps == 6)
        hiddenPreviews = HiddenPreviewsAlways;

    unredirectFullscreen = config.readEntry("UnredirectFullscreen", false);
    animationSpeed = qBound(0, config.readEntry("AnimationSpeed", 3), 6);
}


ElectricBorderAction Options::electricBorderAction(const QString& name)
{
    QString lowerName = name.toLower();
    if (lowerName == "dashboard") return ElectricActionDashboard;
    else if (lowerName == "showdesktop") return ElectricActionShowDesktop;
    else if (lowerName == "lockscreen") return ElectricActionLockScreen;
    else if (lowerName == "preventscreenlocking") return ElectricActionPreventScreenLocking;
    return ElectricActionNone;
}

// restricted should be true for operations that the user may not be able to repeat
// if the window is moved out of the workspace (e.g. if the user moves a window
// by the titlebar, and moves it too high beneath Kicker at the top edge, they
// may not be able to move it back, unless they know about Alt+LMB)
Options::WindowOperation Options::windowOperation(const QString &name, bool restricted)
{
    if (name == "Move")
        return restricted ? MoveOp : UnrestrictedMoveOp;
    else if (name == "Resize")
        return restricted ? ResizeOp : UnrestrictedResizeOp;
    else if (name == "Maximize")
        return MaximizeOp;
    else if (name == "Minimize")
        return MinimizeOp;
    else if (name == "Close")
        return CloseOp;
    else if (name == "OnAllDesktops")
        return OnAllDesktopsOp;
    else if (name == "Shade")
        return ShadeOp;
    else if (name == "Operations")
        return OperationsOp;
    else if (name == "Maximize (vertical only)")
        return VMaximizeOp;
    else if (name == "Maximize (horizontal only)")
        return HMaximizeOp;
    else if (name == "Lower")
        return LowerOp;
    return NoOp;
}

Options::MouseCommand Options::mouseCommand(const QString &name, bool restricted)
{
    QString lowerName = name.toLower();
    if (lowerName == "raise") return MouseRaise;
    if (lowerName == "lower") return MouseLower;
    if (lowerName == "operations menu") return MouseOperationsMenu;
    if (lowerName == "toggle raise and lower") return MouseToggleRaiseAndLower;
    if (lowerName == "activate and raise") return MouseActivateAndRaise;
    if (lowerName == "activate and lower") return MouseActivateAndLower;
    if (lowerName == "activate") return MouseActivate;
    if (lowerName == "activate, raise and pass click") return MouseActivateRaiseAndPassClick;
    if (lowerName == "activate and pass click") return MouseActivateAndPassClick;
    if (lowerName == "scroll") return MouseNothing;
    if (lowerName == "activate and scroll") return MouseActivateAndPassClick;
    if (lowerName == "activate, raise and scroll") return MouseActivateRaiseAndPassClick;
    if (lowerName == "activate, raise and move")
        return restricted ? MouseActivateRaiseAndMove : MouseActivateRaiseAndUnrestrictedMove;
    if (lowerName == "move") return restricted ? MouseMove : MouseUnrestrictedMove;
    if (lowerName == "resize") return restricted ? MouseResize : MouseUnrestrictedResize;
    if (lowerName == "shade") return MouseShade;
    if (lowerName == "minimize") return MouseMinimize;
    if (lowerName == "start window tab drag") return MouseClientGroupDrag;
    if (lowerName == "close") return MouseClose;
    if (lowerName == "nothing") return MouseNothing;
    return MouseNothing;
}

Options::MouseWheelCommand Options::mouseWheelCommand(const QString &name)
{
    QString lowerName = name.toLower();
    if (lowerName == "raise/lower") return MouseWheelRaiseLower;
    if (lowerName == "shade/unshade") return MouseWheelShadeUnshade;
    if (lowerName == "maximize/restore") return MouseWheelMaximizeRestore;
    if (lowerName == "above/below") return MouseWheelAboveBelow;
    if (lowerName == "previous/next desktop") return MouseWheelPreviousNextDesktop;
    if (lowerName == "change opacity") return MouseWheelChangeOpacity;
    if (lowerName == "switch to window tab to the left/right") return MouseWheelChangeGroupWindow;
    if (lowerName == "nothing") return MouseWheelNothing;
    return MouseWheelChangeGroupWindow;
}

bool Options::showGeometryTip()
{
    return show_geometry_tip;
}

ElectricBorderAction Options::electricBorderAction(ElectricBorder edge)
{
    switch(edge) {
    case ElectricTop:
        return electric_border_top;
    case ElectricTopRight:
        return electric_border_top_right;
    case ElectricRight:
        return electric_border_right;
    case ElectricBottomRight:
        return electric_border_bottom_right;
    case ElectricBottom:
        return electric_border_bottom;
    case ElectricBottomLeft:
        return electric_border_bottom_left;
    case ElectricLeft:
        return electric_border_left;
    case ElectricTopLeft:
        return electric_border_top_left;
    default:
        // fallthrough
        break;
    }
    return ElectricActionNone;
}

int Options::electricBorders()
{
    return electric_borders;
}

int Options::electricBorderDelay()
{
    return electric_border_delay;
}

int Options::electricBorderCooldown()
{
    return electric_border_cooldown;
}

bool Options::checkIgnoreFocusStealing(const Client* c)
{
    return ignoreFocusStealingClasses.contains(QString::fromLatin1(c->resourceClass()));
}

Options::MouseCommand Options::wheelToMouseCommand(MouseWheelCommand com, int delta)
{
    switch(com) {
    case MouseWheelRaiseLower:
        return delta > 0 ? MouseRaise : MouseLower;
    case MouseWheelShadeUnshade:
        return delta > 0 ? MouseSetShade : MouseUnsetShade;
    case MouseWheelMaximizeRestore:
        return delta > 0 ? MouseMaximize : MouseRestore;
    case MouseWheelAboveBelow:
        return delta > 0 ? MouseAbove : MouseBelow;
    case MouseWheelPreviousNextDesktop:
        return delta > 0 ? MousePreviousDesktop : MouseNextDesktop;
    case MouseWheelChangeOpacity:
        return delta > 0 ? MouseOpacityMore : MouseOpacityLess;
    case MouseWheelChangeGroupWindow:
        return delta > 0 ? MouseLeftGroupWindow : MouseRightGroupWindow;
    default:
        return MouseNothing;
    }
}
#endif

double Options::animationTimeFactor() const
{
    const double factors[] = { 0, 0.2, 0.5, 1, 2, 4, 20 };
    return factors[ animationSpeed ];
}

} // namespace
