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

class Options : public QObject, public KDecorationOptions
{
    Q_OBJECT
    Q_ENUMS(FocusPolicy)
    Q_ENUMS(MouseCommand)
    Q_ENUMS(MouseWheelCommand)

    Q_PROPERTY(FocusPolicy focusPolicy READ focusPolicy NOTIFY configChanged)
    Q_PROPERTY(bool nextFocusPrefersMouse READ isNextFocusPrefersMouse NOTIFY configChanged)
    /**
       Whether clicking on a window raises it in FocusFollowsMouse
       mode or not.
     */
    Q_PROPERTY(bool clickRaise READ isClickRaise NOTIFY configChanged)
    /**
       whether autoraise is enabled FocusFollowsMouse mode or not.
     */
    Q_PROPERTY(bool autoRaise READ isAutoRaise NOTIFY configChanged)
    /**
       autoraise interval
     */
    Q_PROPERTY(int autoRaiseInterval READ autoRaiseInterval NOTIFY configChanged)
    /**
       delayed focus interval
     */
    Q_PROPERTY(int delayFocusInterval READ delayFocusInterval NOTIFY configChanged)
    /**
       Whether shade hover is enabled or not
     */
    Q_PROPERTY(bool shadeHover READ isShadeHover NOTIFY configChanged)
    /**
       shade hover interval
     */
    Q_PROPERTY(int shadeHoverInterval READ shadeHoverInterval NOTIFY configChanged)
    /**
     * Whether tiling is enabled or not
     */
    Q_PROPERTY(bool tiling  READ isTilingOn WRITE setTilingOn NOTIFY configChanged)
    Q_PROPERTY(int tilingLayout READ tilingLayout NOTIFY configChanged)
    /**
     * Tiling window raise policy.
     */
    Q_PROPERTY(int tilingRaisePolicy READ tilingRaisePolicy NOTIFY configChanged)
    /**
     * whether to see Xinerama screens separately for focus (in Alt+Tab, when activating next client)
     **/
    Q_PROPERTY(bool separateScreenFocus READ isSeparateScreenFocus NOTIFY configChanged)
    /**
     * whether active Xinerama screen is the one with mouse (or with the active window)
     **/
    Q_PROPERTY(bool activeMouseScreen READ isActiveMouseScreen NOTIFY configChanged)
    Q_PROPERTY(int placement READ placement NOTIFY configChanged)
    Q_PROPERTY(bool focusPolicyIsReasonable READ focusPolicyIsReasonable NOTIFY configChanged)
    /**
     * the size of the zone that triggers snapping on desktop borders
     */
    Q_PROPERTY(int borderSnapZone READ borderSnapZone NOTIFY configChanged)
    /**
     * the size of the zone that triggers snapping with other windows
     */
    Q_PROPERTY(int windowSnapZone READ windowSnapZone NOTIFY configChanged)
    /**
     * the size of the zone that triggers snapping on the screen center
     */
    Q_PROPERTY(int centerSnapZone READ centerSnapZone NOTIFY configChanged)
    /**
     * snap only when windows will overlap
     */
    Q_PROPERTY(bool snapOnlyWhenOverlapping READ isSnapOnlyWhenOverlapping NOTIFY configChanged)
    Q_PROPERTY(bool showDesktopIsMinimizeAll READ isShowDesktopIsMinimizeAll NOTIFY configChanged)
    /**
     * whether or not we roll over to the other edge when switching desktops past the edge
     */
    Q_PROPERTY(bool rollOverDesktops READ isRollOverDesktops NOTIFY configChanged)
    /**
     * 0 - 4 , see Workspace::allowClientActivation()
     **/
    Q_PROPERTY(int focusStealingPreventionLevel READ focusStealingPreventionLevel NOTIFY configChanged)
    /**
     * List of window classes to ignore PPosition size hint
     */
    Q_PROPERTY(QStringList ignorePositionClasses READ ignorePositionClasses NOTIFY configChanged)
    /**
    * support legacy fullscreen windows hack: borderless non-netwm windows with screen geometry
    */
    Q_PROPERTY(bool legacyFullscreenSupport READ isLegacyFullscreenSupport NOTIFY configChanged)
    Q_PROPERTY(WindowOperation operationTitlebarDblClick READ operationTitlebarDblClick NOTIFY configChanged)
    Q_PROPERTY(MouseCommand commandActiveTitlebar1 READ commandActiveTitlebar1 NOTIFY configChanged)
    Q_PROPERTY(MouseCommand commandActiveTitlebar2 READ commandActiveTitlebar2 NOTIFY configChanged)
    Q_PROPERTY(MouseCommand commandActiveTitlebar3 READ commandActiveTitlebar3 NOTIFY configChanged)
    Q_PROPERTY(MouseCommand commandInactiveTitlebar1 READ commandInactiveTitlebar1 NOTIFY configChanged)
    Q_PROPERTY(MouseCommand commandInactiveTitlebar2 READ commandInactiveTitlebar2 NOTIFY configChanged)
    Q_PROPERTY(MouseCommand commandInactiveTitlebar3 READ commandInactiveTitlebar3 NOTIFY configChanged)
    Q_PROPERTY(MouseCommand commandWindow1 READ commandWindow1 NOTIFY configChanged)
    Q_PROPERTY(MouseCommand commandWindow2 READ commandWindow2 NOTIFY configChanged)
    Q_PROPERTY(MouseCommand commandWindow3 READ commandWindow3 NOTIFY configChanged)
    Q_PROPERTY(MouseCommand commandWindowWheel READ commandWindowWheel NOTIFY configChanged)
    Q_PROPERTY(MouseCommand commandAll1 READ commandAll1 NOTIFY configChanged)
    Q_PROPERTY(MouseCommand commandAll2 READ commandAll2 NOTIFY configChanged)
    Q_PROPERTY(MouseCommand commandAll3 READ commandAll3 NOTIFY configChanged)
    Q_PROPERTY(uint keyCmdAllModKey READ keyCmdAllModKey NOTIFY configChanged)
    /**
    * whether the Geometry Tip should be shown during a window move/resize.
    */
    Q_PROPERTY(bool showGeometryTip READ showGeometryTip NOTIFY configChanged)
    /**
    * Whether electric borders are enabled. With electric borders
    * you can change desktop by moving the mouse pointer towards the edge
    * of the screen
    */
    Q_PROPERTY(bool electricBorders READ electricBorders NOTIFY configChanged)
    /**
    * the activation delay for electric borders in milliseconds.
    */
    Q_PROPERTY(int electricBorderDelay READ electricBorderDelay NOTIFY configChanged)
    /**
    * the trigger cooldown for electric borders in milliseconds.
    */
    Q_PROPERTY(int electricBorderCooldown READ electricBorderCooldown NOTIFY configChanged)
    /**
    * the number of pixels the mouse cursor is pushed back when it reaches the screen edge.
    */
    Q_PROPERTY(int electricBorderPushbackPixels READ electricBorderPushbackPixels NOTIFY configChanged)
    /**
    * Whether a window gets maximized when it reaches top screen edge while being moved.
    */
    Q_PROPERTY(bool electricBorderMaximize READ electricBorderMaximize NOTIFY configChanged)
    /**
    * Whether a window is tiled to half screen when reaching left or right screen edge while been moved
    */
    Q_PROPERTY(bool electricBorderTiling READ electricBorderTiling NOTIFY configChanged)
    Q_PROPERTY(bool borderlessMaximizedWindows READ borderlessMaximizedWindows NOTIFY configChanged)
    /**
     * timeout before non-responding application will be killed after attempt to close
     **/
    Q_PROPERTY(int killPingTimeout READ killPingTimeout NOTIFY configChanged)
    /**
     * Whether to hide utility windows for inactive applications.
     **/
    Q_PROPERTY(bool hideUtilityWindowsForInactive READ isHideUtilityWindowsForInactive NOTIFY configChanged)
    Q_PROPERTY(bool inactiveTabsSkipTaskbar READ isInactiveTabsSkipTaskbar NOTIFY configChanged)
    Q_PROPERTY(bool autogroupSimilarWindows READ isAutogroupSimilarWindows NOTIFY configChanged)
    Q_PROPERTY(bool autogroupInForeground READ isAutogroupInForeground NOTIFY configChanged)
    Q_PROPERTY(int compositingMode READ compositingMode NOTIFY configChanged)
    Q_PROPERTY(bool useCompositing READ isUseCompositing NOTIFY configChanged)
    Q_PROPERTY(bool compositingInitialized READ isCompositingInitialized WRITE setCompositingInitialized NOTIFY configChanged)
    Q_PROPERTY(int hiddenPreviews READ hiddenPreviews NOTIFY configChanged)
    Q_PROPERTY(bool unredirectFullscreen READ isUnredirectFullscreen NOTIFY configChanged)
    /**
     * 0 = no, 1 = yes when transformed,
     * 2 = try trilinear when transformed; else 1,
     * -1 = auto
     **/
    Q_PROPERTY(int glSmoothScale READ glSmoothScale NOTIFY configChanged)
    Q_PROPERTY(bool glVSync READ isGlVSync NOTIFY configChanged)
    Q_PROPERTY(bool xrenderSmoothScale READ isXrenderSmoothScale NOTIFY configChanged)
    Q_PROPERTY(uint maxFpsInterval READ maxFpsInterval NOTIFY configChanged)
    Q_PROPERTY(uint refreshRate READ refreshRate NOTIFY configChanged)
    Q_PROPERTY(bool glDirect READ isGlDirect NOTIFY configChanged)
    Q_PROPERTY(bool glStrictBinding READ isGlStrictBinding NOTIFY configChanged)
public:

    Options(QObject *parent = NULL);
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
    FocusPolicy focusPolicy() const {
        return m_focusPolicy;
    }
    bool isNextFocusPrefersMouse() const {
        return m_nextFocusPrefersMouse;
    }

    /**
       Whether clicking on a window raises it in FocusFollowsMouse
       mode or not.
     */
    bool isClickRaise() const {
        return m_clickRaise;
    }

    /**
       whether autoraise is enabled FocusFollowsMouse mode or not.
     */
    bool isAutoRaise() const {
        return m_autoRaise;
    }

    /**
       autoraise interval
     */
    int autoRaiseInterval() const {
        return m_autoRaiseInterval;
    }

    /**
       delayed focus interval
     */
    int delayFocusInterval() const {
        return m_delayFocusInterval;
    }

    /**
       Whether shade hover is enabled or not
     */
    bool isShadeHover() const {
        return m_shadeHover;
    }

    /**
       shade hover interval
     */
    int shadeHoverInterval() {
        return m_shadeHoverInterval;
    }

    /**
     * Whether tiling is enabled or not
     */
    bool isTilingOn() const {
        return m_tilingOn;
    }
    void setTilingOn(bool enabled) {
        m_tilingOn = enabled;
    }

    /**
     * Tiling Layout
     */
    TilingLayoutFactory::Layouts tilingLayout() const {
        return m_tilingLayout;
    }

    /**
     * Tiling window raise policy.
     */
    int tilingRaisePolicy() const {
        return m_tilingRaisePolicy;
    }

    // whether to see Xinerama screens separately for focus (in Alt+Tab, when activating next client)
    bool isSeparateScreenFocus() const {
        return m_separateScreenFocus;
    }
    // whether active Xinerama screen is the one with mouse (or with the active window)
    bool isActiveMouseScreen() const {
        return m_activeMouseScreen;
    }

    Placement::Policy placement() const {
        return m_placement;
    }

    bool focusPolicyIsReasonable() {
        return m_focusPolicy == ClickToFocus || m_focusPolicy == FocusFollowsMouse;
    }

    /**
     * the size of the zone that triggers snapping on desktop borders
     */
    int borderSnapZone() const {
        return m_borderSnapZone;
    }

    /**
     * the size of the zone that triggers snapping with other windows
     */
    int windowSnapZone() const {
        return m_windowSnapZone;
    }

    /**
     * the size of the zone that triggers snapping on the screen center
     */
    int centerSnapZone() const {
        return m_centerSnapZone;
    }


    /**
     * snap only when windows will overlap
     */
    bool isSnapOnlyWhenOverlapping() const {
        return m_snapOnlyWhenOverlapping;
    }

    bool isShowDesktopIsMinimizeAll() const {
        return m_showDesktopIsMinimizeAll;
    }

    /**
     * whether or not we roll over to the other edge when switching desktops past the edge
     */
    bool isRollOverDesktops() const {
        return m_rollOverDesktops;
    }

    // 0 - 4 , see Workspace::allowClientActivation()
    int focusStealingPreventionLevel() const {
        return m_focusStealingPreventionLevel;
    }

    /**
     * List of window classes to ignore PPosition size hint
     */
    const QStringList &ignorePositionClasses() const {
        return m_ignorePositionClasses;
    }

    /**
    * support legacy fullscreen windows hack: borderless non-netwm windows with screen geometry
    */
    bool isLegacyFullscreenSupport() const {
        return m_legacyFullscreenSupport;
    }

    bool checkIgnoreFocusStealing(const Client* c) const;

    WindowOperation operationTitlebarDblClick() const {
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
        MouseClose, MousePreviousTab, MouseNextTab, MouseDragTab,
        MouseNothing
    };

    enum MouseWheelCommand {
        MouseWheelRaiseLower, MouseWheelShadeUnshade, MouseWheelMaximizeRestore,
        MouseWheelAboveBelow, MouseWheelPreviousNextDesktop,
        MouseWheelChangeOpacity, MouseWheelChangeCurrentTab,
        MouseWheelNothing
    };

    MouseCommand operationTitlebarMouseWheel(int delta) const {
        return wheelToMouseCommand(CmdTitlebarWheel, delta);
    }
    MouseCommand operationWindowMouseWheel(int delta) const {
        return wheelToMouseCommand(CmdAllWheel, delta);
    }

    MouseCommand commandActiveTitlebar1() const {
        return CmdActiveTitlebar1;
    }
    MouseCommand commandActiveTitlebar2() const {
        return CmdActiveTitlebar2;
    }
    MouseCommand commandActiveTitlebar3() const {
        return CmdActiveTitlebar3;
    }
    MouseCommand commandInactiveTitlebar1() const {
        return CmdInactiveTitlebar1;
    }
    MouseCommand commandInactiveTitlebar2() const {
        return CmdInactiveTitlebar2;
    }
    MouseCommand commandInactiveTitlebar3() const {
        return CmdInactiveTitlebar3;
    }
    MouseCommand commandWindow1() const {
        return CmdWindow1;
    }
    MouseCommand commandWindow2() const {
        return CmdWindow2;
    }
    MouseCommand commandWindow3() const {
        return CmdWindow3;
    }
    MouseCommand commandWindowWheel() const {
        return CmdWindowWheel;
    }
    MouseCommand commandAll1() const {
        return CmdAll1;
    }
    MouseCommand commandAll2() const {
        return CmdAll2;
    }
    MouseCommand commandAll3() const {
        return CmdAll3;
    }
    uint keyCmdAllModKey() const {
        return CmdAllModKey;
    }

    static ElectricBorderAction electricBorderAction(const QString& name);
    static WindowOperation windowOperation(const QString &name, bool restricted);
    static MouseCommand mouseCommand(const QString &name, bool restricted);
    static MouseWheelCommand mouseWheelCommand(const QString &name);

    /**
    * @returns true if the Geometry Tip should be shown during a window move/resize.
    */
    bool showGeometryTip() const;

    enum { ElectricDisabled = 0, ElectricMoveOnly = 1, ElectricAlways = 2 };
    /**
    * @returns The action assigned to the specified electric border
    */
    ElectricBorderAction electricBorderAction(ElectricBorder edge) const;
    /**
    * @returns true if electric borders are enabled. With electric borders
    * you can change desktop by moving the mouse pointer towards the edge
    * of the screen
    */
    int electricBorders() const;
    /**
    * @returns the activation delay for electric borders in milliseconds.
    */
    int electricBorderDelay() const;
    /**
    * @returns the trigger cooldown for electric borders in milliseconds.
    */
    int electricBorderCooldown() const;
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
    int killPingTimeout() const {
        return m_killPingTimeout;
    }

    // Whether to hide utility windows for inactive applications.
    bool isHideUtilityWindowsForInactive() const {
        return m_hideUtilityWindowsForInactive;
    }

    bool isInactiveTabsSkipTaskbar() const {
        return m_inactiveTabsSkipTaskbar;
    }
    bool isAutogroupSimilarWindows() const {
        return m_autogroupSimilarWindows;
    }
    bool isAutogroupInForeground() const {
        return m_autogroupInForeground;
    }

    // Desktop effects
    double animationTimeFactor() const;

    //----------------------
    // Compositing settings
    void reloadCompositingSettings(bool force = false);
    CompositingType compositingMode() const {
        return m_compositingMode;
    }
    void setCompositingMode(CompositingType mode) {
        m_compositingMode = mode;
    }
    // Separate to mode so the user can toggle
    bool isUseCompositing() const {
        return m_useCompositing;
    }
    bool isCompositingInitialized() const {
        return m_compositingInitialized;
    }
    void setCompositingInitialized(bool set) {
        m_compositingInitialized = set;
    }

    // General preferences
    HiddenPreviews hiddenPreviews() const {
        return m_hiddenPreviews;
    }
    bool isUnredirectFullscreen() const {
        return m_unredirectFullscreen;
    }
    // OpenGL
    // 0 = no, 1 = yes when transformed,
    // 2 = try trilinear when transformed; else 1,
    // -1 = auto
    int glSmoothScale() const {
        return m_glSmoothScale;
    }
    bool isGlVSync() const {
        return m_glVSync;
    }
    // XRender
    bool isXrenderSmoothScale() const {
        return m_xrenderSmoothScale;
    }

    uint maxFpsInterval() const {
        return m_maxFpsInterval;
    }
    // Settings that should be auto-detected
    uint refreshRate() const {
        return m_refreshRate;
    }
    bool isGlDirect() const {
        return m_glDirect;
    }
    bool isGlStrictBinding() const {
        return m_glStrictBinding;
    }

    //----------------------
Q_SIGNALS:
    void configChanged();

private:
    FocusPolicy m_focusPolicy;
    bool m_nextFocusPrefersMouse;
    bool m_clickRaise;
    bool m_autoRaise;
    int m_autoRaiseInterval;
    int m_delayFocusInterval;
    bool m_shadeHover;
    int m_shadeHoverInterval;
    bool m_tilingOn;
    TilingLayoutFactory::Layouts m_tilingLayout;
    int m_tilingRaisePolicy;
    bool m_separateScreenFocus;
    bool m_activeMouseScreen;
    Placement::Policy m_placement;
    int m_borderSnapZone;
    int m_windowSnapZone;
    int m_centerSnapZone;
    bool m_snapOnlyWhenOverlapping;
    bool m_showDesktopIsMinimizeAll;
    bool m_rollOverDesktops;
    int m_focusStealingPreventionLevel;
    QStringList m_ignorePositionClasses;
    bool m_legacyFullscreenSupport;
    int m_killPingTimeout;
    bool m_hideUtilityWindowsForInactive;
    bool m_inactiveTabsSkipTaskbar;
    bool m_autogroupSimilarWindows;
    bool m_autogroupInForeground;

    CompositingType m_compositingMode;
    bool m_useCompositing;
    bool m_compositingInitialized;
    HiddenPreviews m_hiddenPreviews;
    bool m_unredirectFullscreen;
    int m_glSmoothScale;
    bool m_glVSync;
    bool m_xrenderSmoothScale;
    uint m_maxFpsInterval;
    // Settings that should be auto-detected
    uint m_refreshRate;
    bool m_glDirect;
    bool m_glStrictBinding;

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

    MouseCommand wheelToMouseCommand(MouseWheelCommand com, int delta) const;
};

extern Options* options;

} // namespace

#endif
