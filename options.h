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

#include "main.h"
#include "placement.h"

namespace KWin
{

// Whether to keep all windows mapped when compositing (i.e. whether to have
// actively updated window pixmaps).
enum HiddenPreviews {
    // The normal mode with regard to mapped windows. Hidden (minimized, etc.)
    // and windows on inactive virtual desktops are not mapped, their pixmaps
    // are only their icons.
    HiddenPreviewsNever,
    // Like normal mode, but shown windows (i.e. on inactive virtual desktops)
    // are kept mapped, only hidden windows are unmapped.
    HiddenPreviewsShown,
    // All windows are kept mapped regardless of their state.
    HiddenPreviewsAlways
};

class Settings;


/**
 * Tuple of a physical device index, vendor ID and device ID.
 */
class VulkanDeviceId
{
public:
    VulkanDeviceId(uint32_t index, uint32_t vendorId, uint32_t deviceId)
        : m_index(index), m_vendorId(vendorId), m_deviceId(deviceId) {}

    uint32_t index() const { return m_index; }
    uint32_t vendorId() const { return m_vendorId; }
    uint32_t deviceId() const { return m_deviceId; }

    bool operator == (const VulkanDeviceId &other) const {
        return m_index == other.m_index &&
               m_vendorId == other.m_vendorId &&
               m_deviceId == other.m_deviceId;
    }

    bool operator != (const VulkanDeviceId &other) const {
        return m_index != other.m_index ||
               m_vendorId != other.m_vendorId ||
               m_deviceId != other.m_deviceId;
    }

private:
    uint32_t m_index;
    uint32_t m_vendorId;
    uint32_t m_deviceId;
};


class KWIN_EXPORT Options : public QObject
{
    Q_OBJECT
    Q_ENUMS(FocusPolicy)
    Q_ENUMS(GlSwapStrategy)
    Q_ENUMS(MouseCommand)
    Q_ENUMS(MouseWheelCommand)
    Q_ENUMS(WindowOperation)

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
     * whether to see Xinerama screens separately for focus (in Alt+Tab, when activating next client)
     **/
    Q_PROPERTY(bool separateScreenFocus READ isSeparateScreenFocus WRITE setSeparateScreenFocus NOTIFY separateScreenFocusChanged)
    Q_PROPERTY(int placement READ placement WRITE setPlacement NOTIFY placementChanged)
    Q_PROPERTY(bool focusPolicyIsReasonable READ focusPolicyIsReasonable NOTIFY focusPolicyIsResonableChanged)
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
    Q_PROPERTY(KWin::Options::WindowOperation operationTitlebarDblClick READ operationTitlebarDblClick WRITE setOperationTitlebarDblClick NOTIFY operationTitlebarDblClickChanged)
    Q_PROPERTY(KWin::Options::WindowOperation operationMaxButtonLeftClick READ operationMaxButtonLeftClick WRITE setOperationMaxButtonLeftClick NOTIFY operationMaxButtonLeftClickChanged)
    Q_PROPERTY(KWin::Options::WindowOperation operationMaxButtonMiddleClick READ operationMaxButtonMiddleClick WRITE setOperationMaxButtonMiddleClick NOTIFY operationMaxButtonMiddleClickChanged)
    Q_PROPERTY(KWin::Options::WindowOperation operationMaxButtonRightClick READ operationMaxButtonRightClick WRITE setOperationMaxButtonRightClick NOTIFY operationMaxButtonRightClickChanged)
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
    * whether the visible name should be condensed
    */
    Q_PROPERTY(bool condensedTitle READ condensedTitle WRITE setCondensedTitle NOTIFY condensedTitleChanged)
    /**
    * Whether a window gets maximized when it reaches top screen edge while being moved.
    */
    Q_PROPERTY(bool electricBorderMaximize READ electricBorderMaximize WRITE setElectricBorderMaximize NOTIFY electricBorderMaximizeChanged)
    /**
    * Whether a window is tiled to half screen when reaching left or right screen edge while been moved
    */
    Q_PROPERTY(bool electricBorderTiling READ electricBorderTiling WRITE setElectricBorderTiling NOTIFY electricBorderTilingChanged)
    /**
    * Whether a window is tiled to half screen when reaching left or right screen edge while been moved
    */
    Q_PROPERTY(float electricBorderCornerRatio READ electricBorderCornerRatio WRITE setElectricBorderCornerRatio NOTIFY electricBorderCornerRatioChanged)
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
    /**
     * 0 = no, 1 = yes when transformed,
     * 2 = try trilinear when transformed; else 1,
     * -1 = auto
     **/
    Q_PROPERTY(int glSmoothScale READ glSmoothScale WRITE setGlSmoothScale NOTIFY glSmoothScaleChanged)
    Q_PROPERTY(bool xrenderSmoothScale READ isXrenderSmoothScale WRITE setXrenderSmoothScale NOTIFY xrenderSmoothScaleChanged)
    Q_PROPERTY(qint64 maxFpsInterval READ maxFpsInterval WRITE setMaxFpsInterval NOTIFY maxFpsIntervalChanged)
    Q_PROPERTY(uint refreshRate READ refreshRate WRITE setRefreshRate NOTIFY refreshRateChanged)
    Q_PROPERTY(qint64 vBlankTime READ vBlankTime WRITE setVBlankTime NOTIFY vBlankTimeChanged)
    Q_PROPERTY(bool glStrictBinding READ isGlStrictBinding WRITE setGlStrictBinding NOTIFY glStrictBindingChanged)
    /**
     * Whether strict binding follows the driver or has been overwritten by a user defined config value.
     * If @c true @link glStrictBinding is set by the OpenGL Scene during initialization.
     * If @c false @link glStrictBinding is set from a config value and not updated during scene initialization.
     **/
    Q_PROPERTY(bool glStrictBindingFollowsDriver READ isGlStrictBindingFollowsDriver WRITE setGlStrictBindingFollowsDriver NOTIFY glStrictBindingFollowsDriverChanged)
    Q_PROPERTY(bool glCoreProfile READ glCoreProfile WRITE setGLCoreProfile NOTIFY glCoreProfileChanged)
    Q_PROPERTY(GlSwapStrategy glPreferBufferSwap READ glPreferBufferSwap WRITE setGlPreferBufferSwap NOTIFY glPreferBufferSwapChanged)
    Q_PROPERTY(VulkanDeviceId vulkanDevice READ vulkanDevice WRITE setVulkanDevice NOTIFY vulkanDeviceChanged)
    Q_PROPERTY(KWin::OpenGLPlatformInterface glPlatformInterface READ glPlatformInterface WRITE setGlPlatformInterface NOTIFY glPlatformInterfaceChanged)
    Q_PROPERTY(bool windowsBlockCompositing READ windowsBlockCompositing WRITE setWindowsBlockCompositing NOTIFY windowsBlockCompositingChanged)
public:

    explicit Options(QObject *parent = NULL);
    ~Options();

    void updateSettings();

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

    // whether to see Xinerama screens separately for focus (in Alt+Tab, when activating next client)
    bool isSeparateScreenFocus() const {
        return m_separateScreenFocus;
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

    enum WindowOperation {
        MaximizeOp = 5000,
        RestoreOp,
        MinimizeOp,
        MoveOp,
        UnrestrictedMoveOp,
        ResizeOp,
        UnrestrictedResizeOp,
        CloseOp,
        OnAllDesktopsOp,
        ShadeOp,
        KeepAboveOp,
        KeepBelowOp,
        OperationsOp,
        WindowRulesOp,
        ToggleStoreSettingsOp = WindowRulesOp, ///< @obsolete
        HMaximizeOp,
        VMaximizeOp,
        LowerOp,
        FullScreenOp,
        NoBorderOp,
        NoOp,
        SetupWindowShortcutOp,
        ApplicationRulesOp,
        RemoveTabFromGroupOp, // Remove from group
        CloseTabGroupOp, // Close the group
        ActivateNextTabOp, // Move left in the group
        ActivatePreviousTabOp, // Move right in the group
        TabDragOp,
    };

    WindowOperation operationTitlebarDblClick() const {
        return OpTitlebarDblClick;
    }
    WindowOperation operationMaxButtonLeftClick() const {
        return opMaxButtonLeftClick;
    }
    WindowOperation operationMaxButtonRightClick() const {
        return opMaxButtonRightClick;
    }
    WindowOperation operationMaxButtonMiddleClick() const {
        return opMaxButtonMiddleClick;
    }
    WindowOperation operationMaxButtonClick(Qt::MouseButtons button) const;


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
    Qt::KeyboardModifier commandAllModifier() const {
        switch (CmdAllModKey) {
        case Qt::Key_Alt:
            return Qt::AltModifier;
        case Qt::Key_Meta:
            return Qt::MetaModifier;
        default:
            Q_UNREACHABLE();
        }
    }

    static WindowOperation windowOperation(const QString &name, bool restricted);
    static MouseCommand mouseCommand(const QString &name, bool restricted);
    static MouseWheelCommand mouseWheelCommand(const QString &name);

    /**
    * @returns true if the Geometry Tip should be shown during a window move/resize.
    */
    bool showGeometryTip() const;

    /**
     * returns whether the user prefers his caption clean
     */
    bool condensedTitle() const;

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
    /**
    * @returns the factor that determines the corner part of the edge (ie. 0.1 means tiny corner)
    */
    float electricBorderCornerRatio() const {
        return electric_border_corner_ratio;
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
    bool isUseCompositing() const;
    bool isCompositingInitialized() const {
        return m_compositingInitialized;
    }

    // General preferences
    HiddenPreviews hiddenPreviews() const {
        return m_hiddenPreviews;
    }
    // OpenGL
    // 0 = no, 1 = yes when transformed,
    // 2 = try trilinear when transformed; else 1,
    // -1 = auto
    int glSmoothScale() const {
        return m_glSmoothScale;
    }
    // XRender
    bool isXrenderSmoothScale() const {
        return m_xrenderSmoothScale;
    }

    qint64 maxFpsInterval() const {
        return m_maxFpsInterval;
    }
    // Settings that should be auto-detected
    uint refreshRate() const {
        return m_refreshRate;
    }
    qint64 vBlankTime() const {
        return m_vBlankTime;
    }
    bool isGlStrictBinding() const {
        return m_glStrictBinding;
    }
    bool isGlStrictBindingFollowsDriver() const {
        return m_glStrictBindingFollowsDriver;
    }
    bool glCoreProfile() const {
        return m_glCoreProfile;
    }
    OpenGLPlatformInterface glPlatformInterface() const {
        return m_glPlatformInterface;
    }

    enum GlSwapStrategy { NoSwapEncourage = 0, CopyFrontBuffer = 'c', PaintFullScreen = 'p', ExtendDamage = 'e', AutoSwapStrategy = 'a' };
    GlSwapStrategy glPreferBufferSwap() const {
        return m_glPreferBufferSwap;
    }

    VulkanDeviceId vulkanDevice() const {
        return m_vulkanDevice;
    }

    bool windowsBlockCompositing() const
    {
        return m_windowsBlockCompositing;
    }

    QStringList modifierOnlyDBusShortcut(Qt::KeyboardModifier mod) const;

    // setters
    void setFocusPolicy(FocusPolicy focusPolicy);
    void setNextFocusPrefersMouse(bool nextFocusPrefersMouse);
    void setClickRaise(bool clickRaise);
    void setAutoRaise(bool autoRaise);
    void setAutoRaiseInterval(int autoRaiseInterval);
    void setDelayFocusInterval(int delayFocusInterval);
    void setShadeHover(bool shadeHover);
    void setShadeHoverInterval(int shadeHoverInterval);
    void setSeparateScreenFocus(bool separateScreenFocus);
    void setPlacement(int placement);
    void setBorderSnapZone(int borderSnapZone);
    void setWindowSnapZone(int windowSnapZone);
    void setCenterSnapZone(int centerSnapZone);
    void setSnapOnlyWhenOverlapping(bool snapOnlyWhenOverlapping);
    void setRollOverDesktops(bool rollOverDesktops);
    void setFocusStealingPreventionLevel(int focusStealingPreventionLevel);
    void setLegacyFullscreenSupport(bool legacyFullscreenSupport);
    void setOperationTitlebarDblClick(WindowOperation operationTitlebarDblClick);
    void setOperationMaxButtonLeftClick(WindowOperation op);
    void setOperationMaxButtonRightClick(WindowOperation op);
    void setOperationMaxButtonMiddleClick(WindowOperation op);
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
    void setCondensedTitle(bool condensedTitle);
    void setElectricBorderMaximize(bool electricBorderMaximize);
    void setElectricBorderTiling(bool electricBorderTiling);
    void setElectricBorderCornerRatio(float electricBorderCornerRatio);
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
    void setGlSmoothScale(int glSmoothScale);
    void setXrenderSmoothScale(bool xrenderSmoothScale);
    void setMaxFpsInterval(qint64 maxFpsInterval);
    void setRefreshRate(uint refreshRate);
    void setVBlankTime(qint64 vBlankTime);
    void setGlStrictBinding(bool glStrictBinding);
    void setGlStrictBindingFollowsDriver(bool glStrictBindingFollowsDriver);
    void setGLCoreProfile(bool glCoreProfile);
    void setGlPreferBufferSwap(char glPreferBufferSwap);
    void setGlPlatformInterface(OpenGLPlatformInterface interface);
    void setVulkanDevice(const VulkanDeviceId &device);
    void setWindowsBlockCompositing(bool set);

    // default values
    static WindowOperation defaultOperationTitlebarDblClick() {
        return MaximizeOp;
    }
    static WindowOperation defaultOperationMaxButtonLeftClick() {
        return MaximizeOp;
    }
    static WindowOperation defaultOperationMaxButtonRightClick() {
        return HMaximizeOp;
    }
    static WindowOperation defaultOperationMaxButtonMiddleClick() {
        return VMaximizeOp;
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
    static int defaultGlSmoothScale() {
        return 2;
    }
    static bool defaultXrenderSmoothScale() {
        return false;
    }
    static qint64 defaultMaxFpsInterval() {
        return (1 * 1000 * 1000 * 1000) /60.0; // nanoseconds / Hz
    }
    static int defaultMaxFps() {
        return 60;
    }
    static uint defaultRefreshRate() {
        return 0;
    }
    static uint defaultVBlankTime() {
        return 6000; // 6ms
    }
    static bool defaultGlStrictBinding() {
        return true;
    }
    static bool defaultGlStrictBindingFollowsDriver() {
        return true;
    }
    static bool defaultGLCoreProfile() {
        return false;
    }
    static GlSwapStrategy defaultGlPreferBufferSwap() {
        return AutoSwapStrategy;
    }
    static VulkanDeviceId defaultVulkanDevice() {
        return VulkanDeviceId(0, 0, 0);
    }
    static OpenGLPlatformInterface defaultGlPlatformInterface() {
        return kwinApp()->shouldUseWaylandForCompositing() ? EglPlatformInterface : GlxPlatformInterface;
    };
    static int defaultAnimationSpeed() {
        return 3;
    }

    /**
     * Performs loading all settings except compositing related.
     **/
    void loadConfig();
    /**
     * Performs loading of compositing settings which do not depend on OpenGL.
     **/
    bool loadCompositingConfig(bool force);
    void reparseConfiguration();

    static int currentRefreshRate();

    //----------------------
Q_SIGNALS:
    // for properties
    void focusPolicyChanged();
    void focusPolicyIsResonableChanged();
    void nextFocusPrefersMouseChanged();
    void clickRaiseChanged();
    void autoRaiseChanged();
    void autoRaiseIntervalChanged();
    void delayFocusIntervalChanged();
    void shadeHoverChanged();
    void shadeHoverIntervalChanged();
    void separateScreenFocusChanged(bool);
    void placementChanged();
    void borderSnapZoneChanged();
    void windowSnapZoneChanged();
    void centerSnapZoneChanged();
    void snapOnlyWhenOverlappingChanged();
    void rollOverDesktopsChanged(bool enabled);
    void focusStealingPreventionLevelChanged();
    void legacyFullscreenSupportChanged();
    void operationTitlebarDblClickChanged();
    void operationMaxButtonLeftClickChanged();
    void operationMaxButtonRightClickChanged();
    void operationMaxButtonMiddleClickChanged();
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
    void condensedTitleChanged();
    void electricBorderMaximizeChanged();
    void electricBorderTilingChanged();
    void electricBorderCornerRatioChanged();
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
    void glSmoothScaleChanged();
    void xrenderSmoothScaleChanged();
    void maxFpsIntervalChanged();
    void refreshRateChanged();
    void vBlankTimeChanged();
    void glStrictBindingChanged();
    void glStrictBindingFollowsDriverChanged();
    void glCoreProfileChanged();
    void glPreferBufferSwapChanged();
    void glPlatformInterfaceChanged();
    void vulkanDeviceChanged();
    void windowsBlockCompositingChanged();

    void configChanged();

private:
    void setElectricBorders(int borders);
    void syncFromKcfgc();
    QScopedPointer<Settings> m_settings;
    FocusPolicy m_focusPolicy;
    bool m_nextFocusPrefersMouse;
    bool m_clickRaise;
    bool m_autoRaise;
    int m_autoRaiseInterval;
    int m_delayFocusInterval;
    bool m_shadeHover;
    int m_shadeHoverInterval;
    bool m_separateScreenFocus;
    Placement::Policy m_placement;
    int m_borderSnapZone;
    int m_windowSnapZone;
    int m_centerSnapZone;
    bool m_snapOnlyWhenOverlapping;
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
    int m_glSmoothScale;
    bool m_xrenderSmoothScale;
    qint64 m_maxFpsInterval;
    // Settings that should be auto-detected
    uint m_refreshRate;
    qint64 m_vBlankTime;
    bool m_glStrictBinding;
    bool m_glStrictBindingFollowsDriver;
    bool m_glCoreProfile;
    GlSwapStrategy m_glPreferBufferSwap;
    OpenGLPlatformInterface m_glPlatformInterface;
    VulkanDeviceId m_vulkanDevice;
    bool m_windowsBlockCompositing;

    WindowOperation OpTitlebarDblClick;
    WindowOperation opMaxButtonRightClick = defaultOperationMaxButtonRightClick();
    WindowOperation opMaxButtonMiddleClick = defaultOperationMaxButtonMiddleClick();
    WindowOperation opMaxButtonLeftClick = defaultOperationMaxButtonRightClick();

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

    bool electric_border_maximize;
    bool electric_border_tiling;
    float electric_border_corner_ratio;
    bool borderless_maximized_windows;
    bool show_geometry_tip;
    bool condensed_title;
    int animationSpeed; // 0 - instant, 5 - very slow

    QHash<Qt::KeyboardModifier, QStringList> m_modifierOnlyShortcuts;

    MouseCommand wheelToMouseCommand(MouseWheelCommand com, int delta) const;
};

extern KWIN_EXPORT Options* options;

} // namespace

Q_DECLARE_METATYPE(KWin::Options::WindowOperation)
Q_DECLARE_METATYPE(KWin::OpenGLPlatformInterface)

#endif
