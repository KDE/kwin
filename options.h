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

#ifndef KWIN_OPTIONS_H
#define KWIN_OPTIONS_H

#include <QObject>
#include <QFont>
#include <QPalette>
#include <kdecoration.h>

#include "placement.h"
#include "utils.h"
#include "tilinglayoutfactory.h"

namespace KWin
{

class Client;
class CompositingPrefs;

class Options : public KDecorationOptions
{
public:

    Options();
    ~Options();

    virtual unsigned long updateSettings();

    /*!
      Different focus policies:
      <ul>

      <li>ClickToFocus - Clicking into a window activates it. This is
      also the default.

      <li>FocusFollowsMouse - Moving the mouse pointer actively onto a
      normal window activates it. For convenience, the desktop and
      windows on the dock are excluded. They require clicking.

      <li>FocusUnderMouse - The window that happens to be under the
      mouse pointer becomes active. The invariant is: no window can
      have focus that is not under the mouse. This also means that
      Alt-Tab won't work properly and popup dialogs are usually
      unsable with the keyboard. Note that the desktop and windows on
      the dock are excluded for convenience. They get focus only when
      clicking on it.

      <li>FocusStrictlyUnderMouse - this is even worse than
      FocusUnderMouse. Only the window under the mouse pointer is
      active. If the mouse points nowhere, nothing has the focus. If
      the mouse points onto the desktop, the desktop has focus. The
      same holds for windows on the dock.

      Note that FocusUnderMouse and FocusStrictlyUnderMouse are not
      particulary useful. They are only provided for old-fashined
      die-hard UNIX people ;-)

      </ul>
     */
    enum FocusPolicy { ClickToFocus, FocusFollowsMouse, FocusUnderMouse, FocusStrictlyUnderMouse };
    FocusPolicy focusPolicy;


    /**
       Whether clicking on a window raises it in FocusFollowsMouse
       mode or not.
     */
    bool clickRaise;

    /**
       whether autoraise is enabled FocusFollowsMouse mode or not.
     */
    bool autoRaise;

    /**
       autoraise interval
     */
    int autoRaiseInterval;

    /**
       whether delay focus is enabled or not.
     */
    bool delayFocus;

    /**
       delayed focus interval
     */
    int delayFocusInterval;

    /**
       Whether shade hover is enabled or not
     */
    bool shadeHover;

    /**
       shade hover interval
     */
    int shadeHoverInterval;

    /**
     * Whether tiling is enabled or not
     */
    bool tilingOn;

    /**
     * Tiling Layout
     */
    enum TilingLayoutFactory::Layouts tilingLayout;

    /**
     * Tiling window raise policy.
     */
    int tilingRaisePolicy;

    // whether to see Xinerama screens separately for focus (in Alt+Tab, when activating next client)
    bool separateScreenFocus;
    // whether active Xinerama screen is the one with mouse (or with the active window)
    bool activeMouseScreen;

    /**
     * Xinerama options
     */
    bool xineramaEnabled;
    bool xineramaPlacementEnabled;
    bool xineramaMovementEnabled;
    bool xineramaMaximizeEnabled;
    bool xineramaFullscreenEnabled;

    // number, or -1 = active screen (Workspace::activeScreen())
    int xineramaPlacementScreen;

    Placement::Policy placement;

    bool focusPolicyIsReasonable() {
        return focusPolicy == ClickToFocus || focusPolicy == FocusFollowsMouse;
    }

    /**
     * the size of the zone that triggers snapping on desktop borders
     */
    int borderSnapZone;

    /**
     * the size of the zone that triggers snapping with other windows
     */
    int windowSnapZone;

    /**
     * the size of the zone that triggers snapping on the screen center
     */
    int centerSnapZone;


    /**
     * snap only when windows will overlap
     */
    bool snapOnlyWhenOverlapping;

    bool showDesktopIsMinimizeAll;

    /**
     * whether or not we roll over to the other edge when switching desktops past the edge
     */
    bool rollOverDesktops;

    // 0 - 4 , see Workspace::allowClientActivation()
    int focusStealingPreventionLevel;

    /**
     * List of window classes to ignore PPosition size hint
     */
    QStringList ignorePositionClasses;

    /**
    * support legacy fullscreen windows hack: borderless non-netwm windows with screen geometry
    */
    bool legacyFullscreenSupport;

    bool checkIgnoreFocusStealing(const Client* c);

    WindowOperation operationTitlebarDblClick() {
        return OpTitlebarDblClick;
    }

    enum MouseCommand {
        MouseRaise, MouseLower, MouseOperationsMenu, MouseToggleRaiseAndLower,
        MouseActivateAndRaise, MouseActivateAndLower, MouseActivate,
        MouseActivateRaiseAndPassClick, MouseActivateAndPassClick,
        MouseMove, MouseUnrestrictedMove,
        MouseActivateRaiseAndMove, MouseActivateRaiseAndUnrestrictedMove,
        MouseResize, MouseUnrestrictedResize,
        MouseShade, MouseSetShade, MouseUnsetShade,
        MouseMaximize, MouseRestore, MouseMinimize,
        MouseNextDesktop, MousePreviousDesktop,
        MouseAbove, MouseBelow,
        MouseOpacityMore, MouseOpacityLess,
        MouseClose, MouseLeftGroupWindow, MouseRightGroupWindow, MouseClientGroupDrag,
        MouseNothing
    };

    enum MouseWheelCommand {
        MouseWheelRaiseLower, MouseWheelShadeUnshade, MouseWheelMaximizeRestore,
        MouseWheelAboveBelow, MouseWheelPreviousNextDesktop,
        MouseWheelChangeOpacity, MouseWheelChangeGroupWindow,
        MouseWheelNothing
    };

    MouseCommand operationTitlebarMouseWheel(int delta) {
        return wheelToMouseCommand(CmdTitlebarWheel, delta);
    }
    MouseCommand operationWindowMouseWheel(int delta) {
        return wheelToMouseCommand(CmdAllWheel, delta);
    }

    MouseCommand commandActiveTitlebar1() {
        return CmdActiveTitlebar1;
    }
    MouseCommand commandActiveTitlebar2() {
        return CmdActiveTitlebar2;
    }
    MouseCommand commandActiveTitlebar3() {
        return CmdActiveTitlebar3;
    }
    MouseCommand commandInactiveTitlebar1() {
        return CmdInactiveTitlebar1;
    }
    MouseCommand commandInactiveTitlebar2() {
        return CmdInactiveTitlebar2;
    }
    MouseCommand commandInactiveTitlebar3() {
        return CmdInactiveTitlebar3;
    }
    MouseCommand commandWindow1() {
        return CmdWindow1;
    }
    MouseCommand commandWindow2() {
        return CmdWindow2;
    }
    MouseCommand commandWindow3() {
        return CmdWindow3;
    }
    MouseCommand commandWindowWheel() {
        return CmdWindowWheel;
    }
    MouseCommand commandAll1() {
        return CmdAll1;
    }
    MouseCommand commandAll2() {
        return CmdAll2;
    }
    MouseCommand commandAll3() {
        return CmdAll3;
    }
    uint keyCmdAllModKey() {
        return CmdAllModKey;
    }

    static ElectricBorderAction electricBorderAction(const QString& name);
    static WindowOperation windowOperation(const QString &name, bool restricted);
    static MouseCommand mouseCommand(const QString &name, bool restricted);
    static MouseWheelCommand mouseWheelCommand(const QString &name);

    /**
    * @returns true if the Geometry Tip should be shown during a window move/resize.
    */
    bool showGeometryTip();

    enum { ElectricDisabled = 0, ElectricMoveOnly = 1, ElectricAlways = 2 };
    /**
    * @returns The action assigned to the specified electric border
    */
    ElectricBorderAction electricBorderAction(ElectricBorder edge);
    /**
    * @returns true if electric borders are enabled. With electric borders
    * you can change desktop by moving the mouse pointer towards the edge
    * of the screen
    */
    int electricBorders();
    /**
    * @returns the activation delay for electric borders in milliseconds.
    */
    int electricBorderDelay();
    /**
    * @returns the trigger cooldown for electric borders in milliseconds.
    */
    int electricBorderCooldown();
    /**
    * @returns the number of pixels the mouse cursor is pushed back when it
    * reaches the screen edge.
    */
    int electricBorderPushbackPixels() const {
        return electric_border_pushback_pixels;
    }
    /**
    * @returns true if a window gets maximized when it reaches top screen edge
    * while being moved.
    */
    bool electricBorderMaximize() const {
        return electric_border_maximize;
    }
    /**
    * @returns true if window is tiled to half screen when reaching left or
    * right screen edge while been moved
    */
    bool electricBorderTiling() const {
        return electric_border_tiling;
    }

    bool borderlessMaximizedWindows() const {
        return borderless_maximized_windows;
    }

    // timeout before non-responding application will be killed after attempt to close
    int killPingTimeout;

    // Whether to hide utility windows for inactive applications.
    bool hideUtilityWindowsForInactive;

    bool inactiveTabsSkipTaskbar;
    bool autogroupSimilarWindows;
    bool autogroupInForeground;

    // Desktop effects
    double animationTimeFactor() const;

    //----------------------
    // Compositing settings
    void reloadCompositingSettings(bool force = false);
    CompositingType compositingMode;
    bool useCompositing; // Separate to mode so the user can toggle
    bool compositingInitialized;

    // General preferences
    HiddenPreviews hiddenPreviews;
    bool unredirectFullscreen;
    bool disableCompositingChecks;
    // OpenGL
    int glSmoothScale;  // 0 = no, 1 = yes when transformed,
    // 2 = try trilinear when transformed; else 1,
    // -1 = auto
    bool glVSync;
    // XRender
    bool xrenderSmoothScale;

    uint maxFpsInterval;
    // Settings that should be auto-detected
    uint refreshRate;
    bool glDirect;
    bool glStrictBinding;

    //----------------------

private:
    WindowOperation OpTitlebarDblClick;

    // mouse bindings
    MouseCommand CmdActiveTitlebar1;
    MouseCommand CmdActiveTitlebar2;
    MouseCommand CmdActiveTitlebar3;
    MouseCommand CmdInactiveTitlebar1;
    MouseCommand CmdInactiveTitlebar2;
    MouseCommand CmdInactiveTitlebar3;
    MouseWheelCommand CmdTitlebarWheel;
    MouseCommand CmdWindow1;
    MouseCommand CmdWindow2;
    MouseCommand CmdWindow3;
    MouseCommand CmdWindowWheel;
    MouseCommand CmdAll1;
    MouseCommand CmdAll2;
    MouseCommand CmdAll3;
    MouseWheelCommand CmdAllWheel;
    uint CmdAllModKey;

    ElectricBorderAction electric_border_top;
    ElectricBorderAction electric_border_top_right;
    ElectricBorderAction electric_border_right;
    ElectricBorderAction electric_border_bottom_right;
    ElectricBorderAction electric_border_bottom;
    ElectricBorderAction electric_border_bottom_left;
    ElectricBorderAction electric_border_left;
    ElectricBorderAction electric_border_top_left;
    int electric_borders;
    int electric_border_delay;
    int electric_border_cooldown;
    int electric_border_pushback_pixels;
    bool electric_border_maximize;
    bool electric_border_tiling;
    bool borderless_maximized_windows;
    bool show_geometry_tip;
    // List of window classes for which not to use focus stealing prevention
    QStringList ignoreFocusStealingClasses;
    int animationSpeed; // 0 - instant, 5 - very slow

    MouseCommand wheelToMouseCommand(MouseWheelCommand com, int delta);
};

extern Options* options;

} // namespace

#endif
