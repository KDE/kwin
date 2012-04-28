/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2012 Martin Gräßlin <m.graesslin@kde.org>

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

    Q_PROPERTY(FocusPolicy focusPolicy READ focusPolicy WRITE setFocusPolicy NOTIFY focusPolicyChanged)
    Q_PROPERTY(bool nextFocusPrefersMouse READ isNextFocusPrefersMouse WRITE setNextFocusPrefersMouse NOTIFY nextFocusPrefersMouseChanged)
    /**
       Whether clicking on a window raises it in FocusFollowsMouse
       mode or not.
     */
    Q_PROPERTY(bool clickRaise READ isClickRaise WRITE setClickRaise NOTIFY clickRaiseChanged)
    /**
       whether autoraise is enabled FocusFollowsMouse mode or not.
     */
    Q_PROPERTY(bool autoRaise READ isAutoRaise WRITE setAutoRaise NOTIFY autoRaiseChanged)
    /**
       autoraise interval
     */
    Q_PROPERTY(int autoRaiseInterval READ autoRaiseInterval WRITE setAutoRaiseInterval NOTIFY autoRaiseIntervalChanged)
    /**
       delayed focus interval
     */
    Q_PROPERTY(int delayFocusInterval READ delayFocusInterval WRITE setDelayFocusInterval NOTIFY delayFocusIntervalChanged)
    /**
       Whether shade hover is enabled or not
     */
    Q_PROPERTY(bool shadeHover READ isShadeHover WRITE setShadeHover NOTIFY shadeHoverChanged)
    /**
       shade hover interval
     */
    Q_PROPERTY(int shadeHoverInterval READ shadeHoverInterval WRITE setShadeHoverInterval NOTIFY shadeHoverIntervalChanged)
    /**
     * Whether tiling is enabled or not
     */
    Q_PROPERTY(bool tiling READ isTilingOn WRITE setTilingOn WRITE setTiling NOTIFY tilingChanged)
    Q_PROPERTY(int tilingLayout READ tilingLayout WRITE setTilingLayout NOTIFY tilingLayoutChanged)
    /**
     * Tiling window raise policy.
     */
    Q_PROPERTY(int tilingRaisePolicy READ tilingRaisePolicy WRITE setTilingRaisePolicy NOTIFY tilingRaisePolicyChanged)
    /**
     * whether to see Xinerama screens separately for focus (in Alt+Tab, when activating next client)
     **/
    Q_PROPERTY(bool separateScreenFocus READ isSeparateScreenFocus WRITE setSeparateScreenFocus NOTIFY separateScreenFocusChanged)
    /**
     * whether active Xinerama screen is the one with mouse (or with the active window)
     **/
    Q_PROPERTY(bool activeMouseScreen READ isActiveMouseScreen WRITE setActiveMouseScreen NOTIFY activeMouseScreenChanged)
    Q_PROPERTY(int placement READ placement WRITE setPlacement NOTIFY placementChanged)
    Q_PROPERTY(bool focusPolicyIsReasonable READ focusPolicyIsReasonable NOTIFY configChanged)
    /**
     * the size of the zone that triggers snapping on desktop borders
     */
    Q_PROPERTY(int borderSnapZone READ borderSnapZone WRITE setBorderSnapZone NOTIFY borderSnapZoneChanged)
    /**
     * the size of the zone that triggers snapping with other windows
     */
    Q_PROPERTY(int windowSnapZone READ windowSnapZone WRITE setWindowSnapZone NOTIFY windowSnapZoneChanged)
    /**
     * the size of the zone that triggers snapping on the screen center
     */
    Q_PROPERTY(int centerSnapZone READ centerSnapZone WRITE setCenterSnapZone NOTIFY centerSnapZoneChanged)
    /**
     * snap only when windows will overlap
     */
    Q_PROPERTY(bool snapOnlyWhenOverlapping READ isSnapOnlyWhenOverlapping WRITE setSnapOnlyWhenOverlapping NOTIFY snapOnlyWhenOverlappingChanged)
    Q_PROPERTY(bool showDesktopIsMinimizeAll READ isShowDesktopIsMinimizeAll WRITE setShowDesktopIsMinimizeAll NOTIFY showDesktopIsMinimizeAllChanged)
    /**
     * whether or not we roll over to the other edge when switching desktops past the edge
     */
    Q_PROPERTY(bool rollOverDesktops READ isRollOverDesktops WRITE setRollOverDesktops NOTIFY rollOverDesktopsChanged)
    /**
     * 0 - 4 , see Workspace::allowClientActivation()
     **/
    Q_PROPERTY(int focusStealingPreventionLevel READ focusStealingPreventionLevel WRITE setFocusStealingPreventionLevel NOTIFY focusStealingPreventionLevelChanged)
    /**
    * support legacy fullscreen windows hack: borderless non-netwm windows with screen geometry
    */
    Q_PROPERTY(bool legacyFullscreenSupport READ isLegacyFullscreenSupport WRITE setLegacyFullscreenSupport NOTIFY legacyFullscreenSupportChanged)
    Q_PROPERTY(WindowOperation operationTitlebarDblClick READ operationTitlebarDblClick WRITE setOperationTitlebarDblClick NOTIFY operationTitlebarDblClickChanged)
    Q_PROPERTY(MouseCommand commandActiveTitlebar1 READ commandActiveTitlebar1 WRITE setCommandActiveTitlebar1 NOTIFY commandActiveTitlebar1Changed)
    Q_PROPERTY(MouseCommand commandActiveTitlebar2 READ commandActiveTitlebar2 WRITE setCommandActiveTitlebar2 NOTIFY commandActiveTitlebar2Changed)
    Q_PROPERTY(MouseCommand commandActiveTitlebar3 READ commandActiveTitlebar3 WRITE setCommandActiveTitlebar3 NOTIFY commandActiveTitlebar3Changed)
    Q_PROPERTY(MouseCommand commandInactiveTitlebar1 READ commandInactiveTitlebar1 WRITE setCommandInactiveTitlebar1 NOTIFY commandInactiveTitlebar1Changed)
    Q_PROPERTY(MouseCommand commandInactiveTitlebar2 READ commandInactiveTitlebar2 WRITE setCommandInactiveTitlebar2 NOTIFY commandInactiveTitlebar2Changed)
    Q_PROPERTY(MouseCommand commandInactiveTitlebar3 READ commandInactiveTitlebar3 WRITE setCommandInactiveTitlebar3 NOTIFY commandInactiveTitlebar3Changed)
    Q_PROPERTY(MouseCommand commandWindow1 READ commandWindow1 WRITE setCommandWindow1 NOTIFY commandWindow1Changed)
    Q_PROPERTY(MouseCommand commandWindow2 READ commandWindow2 WRITE setCommandWindow2 NOTIFY commandWindow2Changed)
    Q_PROPERTY(MouseCommand commandWindow3 READ commandWindow3 WRITE setCommandWindow3 NOTIFY commandWindow3Changed)
    Q_PROPERTY(MouseCommand commandWindowWheel READ commandWindowWheel WRITE setCommandWindowWheel NOTIFY commandWindowWheelChanged)
    Q_PROPERTY(MouseCommand commandAll1 READ commandAll1 WRITE setCommandAll1 NOTIFY commandAll1Changed)
    Q_PROPERTY(MouseCommand commandAll2 READ commandAll2 WRITE setCommandAll2 NOTIFY commandAll2Changed)
    Q_PROPERTY(MouseCommand commandAll3 READ commandAll3 WRITE setCommandAll3 NOTIFY commandAll3Changed)
    Q_PROPERTY(uint keyCmdAllModKey READ keyCmdAllModKey WRITE setKeyCmdAllModKey NOTIFY keyCmdAllModKeyChanged)
    /**
    * whether the Geometry Tip should be shown during a window move/resize.
    */
    Q_PROPERTY(bool showGeometryTip READ showGeometryTip WRITE setShowGeometryTip NOTIFY showGeometryTipChanged)
    /**
    * Whether electric borders are enabled. With electric borders
    * you can change desktop by moving the mouse pointer towards the edge
    * of the screen
    */
    Q_PROPERTY(bool electricBorders READ electricBorders NOTIFY electricBordersChanged)
    /**
    * the activation delay for electric borders in milliseconds.
    */
    Q_PROPERTY(int electricBorderDelay READ electricBorderDelay WRITE setElectricBorderDelay NOTIFY electricBorderDelayChanged)
    /**
    * the trigger cooldown for electric borders in milliseconds.
    */
    Q_PROPERTY(int electricBorderCooldown READ electricBorderCooldown WRITE setElectricBorderCooldown NOTIFY electricBorderCooldownChanged)
    /**
    * the number of pixels the mouse cursor is pushed back when it reaches the screen edge.
    */
    Q_PROPERTY(int electricBorderPushbackPixels READ electricBorderPushbackPixels WRITE setElectricBorderPushbackPixels NOTIFY electricBorderPushbackPixelsChanged)
    /**
    * Whether a window gets maximized when it reaches top screen edge while being moved.
    */
    Q_PROPERTY(bool electricBorderMaximize READ electricBorderMaximize WRITE setElectricBorderMaximize NOTIFY electricBorderMaximizeChanged)
    /**
    * Whether a window is tiled to half screen when reaching left or right screen edge while been moved
    */
    Q_PROPERTY(bool electricBorderTiling READ electricBorderTiling WRITE setElectricBorderTiling NOTIFY electricBorderTilingChanged)
    Q_PROPERTY(bool borderlessMaximizedWindows READ borderlessMaximizedWindows WRITE setBorderlessMaximizedWindows NOTIFY borderlessMaximizedWindowsChanged)
    /**
     * timeout before non-responding application will be killed after attempt to close
     **/
    Q_PROPERTY(int killPingTimeout READ killPingTimeout WRITE setKillPingTimeout NOTIFY killPingTimeoutChanged)
    /**
     * Whether to hide utility windows for inactive applications.
     **/
    Q_PROPERTY(bool hideUtilityWindowsForInactive READ isHideUtilityWindowsForInactive WRITE setHideUtilityWindowsForInactive NOTIFY hideUtilityWindowsForInactiveChanged)
    Q_PROPERTY(bool inactiveTabsSkipTaskbar READ isInactiveTabsSkipTaskbar WRITE setInactiveTabsSkipTaskbar NOTIFY inactiveTabsSkipTaskbarChanged)
    Q_PROPERTY(bool autogroupSimilarWindows READ isAutogroupSimilarWindows WRITE setAutogroupSimilarWindows NOTIFY autogroupSimilarWindowsChanged)
    Q_PROPERTY(bool autogroupInForeground READ isAutogroupInForeground WRITE setAutogroupInForeground NOTIFY autogroupInForegroundChanged)
    Q_PROPERTY(int compositingMode READ compositingMode WRITE setCompositingMode NOTIFY compositingModeChanged)
    Q_PROPERTY(bool useCompositing READ isUseCompositing WRITE setUseCompositing NOTIFY useCompositingChanged)
    Q_PROPERTY(bool compositingInitialized READ isCompositingInitialized WRITE setCompositingInitialized NOTIFY compositingInitializedChanged)
    Q_PROPERTY(int hiddenPreviews READ hiddenPreviews WRITE setHiddenPreviews NOTIFY hiddenPreviewsChanged)
    Q_PROPERTY(bool unredirectFullscreen READ isUnredirectFullscreen WRITE setUnredirectFullscreen NOTIFY unredirectFullscreenChanged)
    /**
     * 0 = no, 1 = yes when transformed,
     * 2 = try trilinear when transformed; else 1,
     * -1 = auto
     **/
    Q_PROPERTY(int glSmoothScale READ glSmoothScale WRITE setGlSmoothScale NOTIFY glSmoothScaleChanged)
    Q_PROPERTY(bool glVSync READ isGlVSync WRITE setGlVSync NOTIFY glVSyncChanged)
    Q_PROPERTY(bool xrenderSmoothScale READ isXrenderSmoothScale WRITE setXrenderSmoothScale NOTIFY xrenderSmoothScaleChanged)
    Q_PROPERTY(uint maxFpsInterval READ maxFpsInterval WRITE setMaxFpsInterval NOTIFY maxFpsIntervalChanged)
    Q_PROPERTY(uint refreshRate READ refreshRate WRITE setRefreshRate NOTIFY refreshRateChanged)
    Q_PROPERTY(bool glDirect READ isGlDirect WRITE setGlDirect NOTIFY glDirectChanged)
    Q_PROPERTY(bool glStrictBinding READ isGlStrictBinding WRITE setGlStrictBinding NOTIFY glStrictBindingChanged)
    /**
     * Whether strict binding follows the driver or has been overwritten by a user defined config value.
     * If @c true @link glStrictBinding is set by the OpenGL Scene during initialization.
     * If @c false @link glStrictBinding is set from a config value and not updated during scene initialization.
     **/
    Q_PROPERTY(bool glStrictBindingFollowsDriver READ isGlStrictBindingFollowsDriver WRITE setGlStrictBindingFollowsDriver NOTIFY glStrictBindingFollowsDriverChanged)
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
    * support legacy fullscreen windows hack: borderless non-netwm windows with screen geometry
    */
    bool isLegacyFullscreenSupport() const {
        return m_legacyFullscreenSupport;
    }

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
    bool isGlStrictBindingFollowsDriver() const {
        return m_glStrictBindingFollowsDriver;
    }

    // setters
    void setFocusPolicy(FocusPolicy focusPolicy);
    void setNextFocusPrefersMouse(bool nextFocusPrefersMouse);
    void setClickRaise(bool clickRaise);
    void setAutoRaise(bool autoRaise);
    void setAutoRaiseInterval(int autoRaiseInterval);
    void setDelayFocusInterval(int delayFocusInterval);
    void setShadeHover(bool shadeHover);
    void setShadeHoverInterval(int shadeHoverInterval);
    void setTiling(bool tiling);
    void setTilingLayout(int tilingLayout);
    void setTilingRaisePolicy(int tilingRaisePolicy);
    void setSeparateScreenFocus(bool separateScreenFocus);
    void setActiveMouseScreen(bool activeMouseScreen);
    void setPlacement(int placement);
    void setBorderSnapZone(int borderSnapZone);
    void setWindowSnapZone(int windowSnapZone);
    void setCenterSnapZone(int centerSnapZone);
    void setSnapOnlyWhenOverlapping(bool snapOnlyWhenOverlapping);
    void setShowDesktopIsMinimizeAll(bool showDesktopIsMinimizeAll);
    void setRollOverDesktops(bool rollOverDesktops);
    void setFocusStealingPreventionLevel(int focusStealingPreventionLevel);
    void setLegacyFullscreenSupport(bool legacyFullscreenSupport);
    void setOperationTitlebarDblClick(WindowOperation operationTitlebarDblClick);
    void setCommandActiveTitlebar1(MouseCommand commandActiveTitlebar1);
    void setCommandActiveTitlebar2(MouseCommand commandActiveTitlebar2);
    void setCommandActiveTitlebar3(MouseCommand commandActiveTitlebar3);
    void setCommandInactiveTitlebar1(MouseCommand commandInactiveTitlebar1);
    void setCommandInactiveTitlebar2(MouseCommand commandInactiveTitlebar2);
    void setCommandInactiveTitlebar3(MouseCommand commandInactiveTitlebar3);
    void setCommandWindow1(MouseCommand commandWindow1);
    void setCommandWindow2(MouseCommand commandWindow2);
    void setCommandWindow3(MouseCommand commandWindow3);
    void setCommandWindowWheel(MouseCommand commandWindowWheel);
    void setCommandAll1(MouseCommand commandAll1);
    void setCommandAll2(MouseCommand commandAll2);
    void setCommandAll3(MouseCommand commandAll3);
    void setKeyCmdAllModKey(uint keyCmdAllModKey);
    void setShowGeometryTip(bool showGeometryTip);
    void setElectricBorderDelay(int electricBorderDelay);
    void setElectricBorderCooldown(int electricBorderCooldown);
    void setElectricBorderPushbackPixels(int electricBorderPushbackPixels);
    void setElectricBorderMaximize(bool electricBorderMaximize);
    void setElectricBorderTiling(bool electricBorderTiling);
    void setBorderlessMaximizedWindows(bool borderlessMaximizedWindows);
    void setKillPingTimeout(int killPingTimeout);
    void setHideUtilityWindowsForInactive(bool hideUtilityWindowsForInactive);
    void setInactiveTabsSkipTaskbar(bool inactiveTabsSkipTaskbar);
    void setAutogroupSimilarWindows(bool autogroupSimilarWindows);
    void setAutogroupInForeground(bool autogroupInForeground);
    void setCompositingMode(int compositingMode);
    void setUseCompositing(bool useCompositing);
    void setCompositingInitialized(bool compositingInitialized);
    void setHiddenPreviews(int hiddenPreviews);
    void setUnredirectFullscreen(bool unredirectFullscreen);
    void setGlSmoothScale(int glSmoothScale);
    void setGlVSync(bool glVSync);
    void setXrenderSmoothScale(bool xrenderSmoothScale);
    void setMaxFpsInterval(uint maxFpsInterval);
    void setRefreshRate(uint refreshRate);
    void setGlDirect(bool glDirect);
    void setGlStrictBinding(bool glStrictBinding);
    void setGlStrictBindingFollowsDriver(bool glStrictBindingFollowsDriver);

    // default values
    static FocusPolicy defaultFocusPolicy() {
        return ClickToFocus;
    }
    static bool defaultNextFocusPrefersMouse() {
        return false;
    }
    static bool defaultClickRaise() {
        return true;
    }
    static bool defaultAutoRaise() {
        return false;
    }
    static int defaultAutoRaiseInterval() {
        return 0;
    }
    static int defaultDelayFocusInterval() {
        return 0;
    }
    static bool defaultShadeHover() {
        return false;
    }
    static int defaultShadeHoverInterval() {
        return 250;
    }
    static bool defaultTiling() {
        return false;
    }
    static TilingLayoutFactory::Layouts defaultTilingLayout() {
        return TilingLayoutFactory::DefaultLayout;
    }
    static int defaultTilingRaisePolicy() {
        return 0;
    }
    static bool defaultSeparateScreenFocus() {
        return false;
    }
    static bool defaultActiveMouseScreen() {
        // TODO: used to be m_focusPolicy != ClickToFocus
        return true;
    }
    static Placement::Policy defaultPlacement() {
        return Placement::Default;
    }
    static int defaultBorderSnapZone() {
        return 10;
    }
    static int defaultWindowSnapZone() {
        return 10;
    }
    static int defaultCenterSnapZone() {
        return 0;
    }
    static bool defaultSnapOnlyWhenOverlapping() {
        return false;
    }
    static bool defaultShowDesktopIsMinimizeAll() {
        return false;
    }
    static bool defaultRollOverDesktops() {
        return true;
    }
    static int defaultFocusStealingPreventionLevel() {
        return 1;
    }
    static bool defaultLegacyFullscreenSupport() {
        return false;
    }
    static WindowOperation defaultOperationTitlebarDblClick() {
        return MaximizeOp;
    }
    static MouseCommand defaultCommandActiveTitlebar1() {
        return MouseRaise;
    }
    static MouseCommand defaultCommandActiveTitlebar2() {
        return MouseDragTab;
    }
    static MouseCommand defaultCommandActiveTitlebar3() {
        return MouseOperationsMenu;
    }
    static MouseCommand defaultCommandInactiveTitlebar1() {
        return MouseActivateAndRaise;
    }
    static MouseCommand defaultCommandInactiveTitlebar2() {
        return MouseDragTab;
    }
    static MouseCommand defaultCommandInactiveTitlebar3() {
        return MouseOperationsMenu;
    }
    static MouseCommand defaultCommandWindow1() {
        return MouseActivateRaiseAndPassClick;
    }
    static MouseCommand defaultCommandWindow2() {
        return MouseActivateAndPassClick;
    }
    static MouseCommand defaultCommandWindow3() {
        return MouseActivateAndPassClick;
    }
    static MouseCommand defaultCommandWindowWheel() {
        return MouseNothing;
    }
    static MouseCommand defaultCommandAll1() {
        return MouseUnrestrictedMove;
    }
    static MouseCommand defaultCommandAll2() {
        return MouseToggleRaiseAndLower;
    }
    static MouseCommand defaultCommandAll3() {
        return MouseUnrestrictedResize;
    }
    static MouseWheelCommand defaultCommandTitlebarWheel() {
        return MouseWheelChangeCurrentTab;
    }
    static MouseWheelCommand defaultCommandAllWheel() {
        return MouseWheelNothing;
    }
    static uint defaultKeyCmdAllModKey() {
        return Qt::Key_Alt;
    }
    static bool defaultShowGeometryTip() {
        return false;
    }
    static ElectricBorderAction defaultElectricBorderTop() {
        return ElectricActionNone;
    }
    static ElectricBorderAction defaultElectricBorderTopRight() {
        return ElectricActionNone;
    }
    static ElectricBorderAction defaultElectricBorderRight() {
        return ElectricActionNone;
    }
    static ElectricBorderAction defaultElectricBorderBottomRight() {
        return ElectricActionNone;
    }
    static ElectricBorderAction defaultElectricBorderBottom() {
        return ElectricActionNone;
    }
    static ElectricBorderAction defaultElectricBorderBottomLeft() {
        return ElectricActionNone;
    }
    static ElectricBorderAction defaultElectricBorderLeft() {
        return ElectricActionNone;
    }
    static ElectricBorderAction defaultElectricBorderTopLeft() {
        return ElectricActionNone;
    }
    static int defaultElectricBorders() {
        return 0;
    }
    static int defaultElectricBorderDelay() {
        return 150;
    }
    static int defaultElectricBorderCooldown() {
        return 350;
    }
    static int defaultElectricBorderPushbackPixels() {
        return 1;
    }
    static bool defaultElectricBorderMaximize() {
        return true;
    }
    static bool defaultElectricBorderTiling() {
        return true;
    }
    static bool defaultBorderlessMaximizedWindows() {
        return false;
    }
    static int defaultKillPingTimeout() {
        return 5000;
    }
    static bool defaultHideUtilityWindowsForInactive() {
        return true;
    }
    static bool defaultInactiveTabsSkipTaskbar() {
        return false;
    }
    static bool defaultAutogroupSimilarWindows() {
        return false;
    }
    static bool defaultAutogroupInForeground() {
        return true;
    }
    static CompositingType defaultCompositingMode() {
        return OpenGLCompositing;
    }
    static bool defaultUseCompositing() {
        return true;
    }
    static bool defaultCompositingInitialized() {
        return false;
    }
    static HiddenPreviews defaultHiddenPreviews() {
        return HiddenPreviewsShown;
    }
    static bool defaultUnredirectFullscreen() {
        return false;
    }
    static int defaultGlSmoothScale() {
        return 2;
    }
    static bool defaultGlVSync() {
        return true;
    }
    static bool defaultXrenderSmoothScale() {
        return false;
    }
    static uint defaultMaxFpsInterval() {
        return qRound(1000.0/60.0);
    }
    static int defaultMaxFps() {
        return 60;
    }
    static uint defaultRefreshRate() {
        return 0;
    }
    static bool defaultGlDirect() {
        return true;
    }
    static bool defaultGlStrictBinding() {
        return true;
    }
    static bool defaultGlStrictBindingFollowsDriver() {
        return true;
    }
    static int defaultAnimationSpeed() {
        return 3;
    }

    /**
     * Performs loading all settings except compositing related.
     **/
    unsigned long loadConfig();
    /**
     * Performs loading of compositing settings which do not depend on OpenGL.
     **/
    bool loadCompositingConfig(bool force);
    void reparseConfiguration();

    //----------------------
Q_SIGNALS:
    void configChanged();

    // for properties
    void focusPolicyChanged();
    void nextFocusPrefersMouseChanged();
    void clickRaiseChanged();
    void autoRaiseChanged();
    void autoRaiseIntervalChanged();
    void delayFocusIntervalChanged();
    void shadeHoverChanged();
    void shadeHoverIntervalChanged();
    void tilingChanged();
    void tilingLayoutChanged();
    void tilingRaisePolicyChanged();
    void separateScreenFocusChanged();
    void activeMouseScreenChanged();
    void placementChanged();
    void borderSnapZoneChanged();
    void windowSnapZoneChanged();
    void centerSnapZoneChanged();
    void snapOnlyWhenOverlappingChanged();
    void showDesktopIsMinimizeAllChanged();
    void rollOverDesktopsChanged();
    void focusStealingPreventionLevelChanged();
    void legacyFullscreenSupportChanged();
    void operationTitlebarDblClickChanged();
    void commandActiveTitlebar1Changed();
    void commandActiveTitlebar2Changed();
    void commandActiveTitlebar3Changed();
    void commandInactiveTitlebar1Changed();
    void commandInactiveTitlebar2Changed();
    void commandInactiveTitlebar3Changed();
    void commandWindow1Changed();
    void commandWindow2Changed();
    void commandWindow3Changed();
    void commandWindowWheelChanged();
    void commandAll1Changed();
    void commandAll2Changed();
    void commandAll3Changed();
    void keyCmdAllModKeyChanged();
    void showGeometryTipChanged();
    void electricBordersChanged();
    void electricBorderDelayChanged();
    void electricBorderCooldownChanged();
    void electricBorderPushbackPixelsChanged();
    void electricBorderMaximizeChanged();
    void electricBorderTilingChanged();
    void borderlessMaximizedWindowsChanged();
    void killPingTimeoutChanged();
    void hideUtilityWindowsForInactiveChanged();
    void inactiveTabsSkipTaskbarChanged();
    void autogroupSimilarWindowsChanged();
    void autogroupInForegroundChanged();
    void compositingModeChanged();
    void useCompositingChanged();
    void compositingInitializedChanged();
    void hiddenPreviewsChanged();
    void unredirectFullscreenChanged();
    void glSmoothScaleChanged();
    void glVSyncChanged();
    void xrenderSmoothScaleChanged();
    void maxFpsIntervalChanged();
    void refreshRateChanged();
    void glDirectChanged();
    void glStrictBindingChanged();
    void glStrictBindingFollowsDriverChanged();

private:
    void setElectricBorders(int borders);
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
    bool m_glStrictBindingFollowsDriver;

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
    int animationSpeed; // 0 - instant, 5 - very slow

    MouseCommand wheelToMouseCommand(MouseWheelCommand com, int delta) const;
};

extern Options* options;

} // namespace

#endif
