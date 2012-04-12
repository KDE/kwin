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

#include <X11/extensions/Xrandr.h>
#ifndef KWIN_HAVE_OPENGLES
#ifndef KWIN_NO_XF86VM
#include <X11/extensions/xf86vmode.h>
#endif
#endif

#endif //KCMRULES

namespace KWin
{

#ifndef KCMRULES

int currentRefreshRate()
{
    int rate = -1;
    if (options->refreshRate() > 0)   // use manually configured refresh rate
        rate = options->refreshRate();
#ifndef KWIN_HAVE_OPENGLES
    else if (GLPlatform::instance()->driver() == Driver_NVidia) {
#ifndef KWIN_NO_XF86VM
        int major, event, error;
        if (XQueryExtension(display(), "XFree86-VidModeExtension", &major, &event, &error)) {
            XF86VidModeModeLine modeline;
            int dotclock, vtotal;
            if (XF86VidModeGetModeLine(display(), 0, &dotclock, &modeline)) {
                vtotal = modeline.vtotal;
                if (modeline.flags & 0x0010) // V_INTERLACE
                    dotclock *= 2;
                if (modeline.flags & 0x0020) // V_DBLSCAN
                    vtotal *= 2;
                rate = 1000*dotclock/(modeline.htotal*vtotal); // WTF was wikipedia 1998 when I nedded it?
                kDebug(1212) << "Vertical Refresh Rate (as detected by XF86VM): " << rate << "Hz";
            }
        }
        if (rate < 1)
#endif
        { // modeline approach failed
            QProcess nvidia_settings;
            QStringList env = QProcess::systemEnvironment();
            env << "LC_ALL=C";
            nvidia_settings.setEnvironment(env);
            nvidia_settings.start("nvidia-settings", QStringList() << "-t" << "-q" << "RefreshRate", QIODevice::ReadOnly);
            nvidia_settings.waitForFinished();
            if (nvidia_settings.exitStatus() == QProcess::NormalExit) {
                QString reply = QString::fromLocal8Bit(nvidia_settings.readAllStandardOutput()).split(' ').first();
                bool ok;
                float frate = QLocale::c().toFloat(reply, &ok);
                if (!ok)
                    rate = -1;
                else
                    rate = qRound(frate);
                kDebug(1212) << "Vertical Refresh Rate (as detected by nvidia-settings): " << rate << "Hz";
            }
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
    kDebug(1212) << "Vertical Refresh rate " << rate << "Hz";
    return rate;
}

Options::Options(QObject *parent)
    : QObject(parent)
    , electric_borders(0)
    , electric_border_delay(0)
{
    m_compositingInitialized = false;
    updateSettings();
}

Options::~Options()
{
}

void Options::setFocusPolicy(FocusPolicy focusPolicy)
{
    if (m_focusPolicy == focusPolicy) {
        return;
    }
    m_focusPolicy = focusPolicy;
    emit focusPolicyChanged();
}

void Options::setNextFocusPrefersMouse(bool nextFocusPrefersMouse)
{
    if (m_nextFocusPrefersMouse == nextFocusPrefersMouse) {
        return;
    }
    m_nextFocusPrefersMouse = nextFocusPrefersMouse;
    emit nextFocusPrefersMouseChanged();
}

void Options::setClickRaise(bool clickRaise)
{
    if (m_clickRaise == clickRaise) {
        return;
    }
    m_clickRaise = clickRaise;
    emit clickRaiseChanged();
}

void Options::setAutoRaise(bool autoRaise)
{
    if (m_autoRaise == autoRaise) {
        return;
    }
    m_autoRaise = autoRaise;
    emit autoRaiseChanged();
}

void Options::setAutoRaiseInterval(int autoRaiseInterval)
{
    if (m_autoRaiseInterval == autoRaiseInterval) {
        return;
    }
    m_autoRaiseInterval = autoRaiseInterval;
    emit autoRaiseIntervalChanged();
}

void Options::setDelayFocusInterval(int delayFocusInterval)
{
    if (m_delayFocusInterval == delayFocusInterval) {
        return;
    }
    m_delayFocusInterval = delayFocusInterval;
    emit delayFocusIntervalChanged();
}

void Options::setShadeHover(bool shadeHover)
{
    if (m_shadeHover == shadeHover) {
        return;
    }
    m_shadeHover = shadeHover;
    emit shadeHoverChanged();
}

void Options::setShadeHoverInterval(int shadeHoverInterval)
{
    if (m_shadeHoverInterval == shadeHoverInterval) {
        return;
    }
    m_shadeHoverInterval = shadeHoverInterval;
    emit shadeHoverIntervalChanged();
}

void Options::setTiling(bool tiling)
{
    if (m_tilingOn == tiling) {
        return;
    }
    m_tilingOn = tiling;
    emit tilingChanged();
}

void Options::setTilingLayout(int tilingLayout)
{
    if (m_tilingLayout == static_cast<TilingLayoutFactory::Layouts>(tilingLayout)) {
        return;
    }
    m_tilingLayout = static_cast<TilingLayoutFactory::Layouts>(tilingLayout);
    emit tilingLayoutChanged();
}

void Options::setTilingRaisePolicy(int tilingRaisePolicy)
{
    if (m_tilingRaisePolicy == tilingRaisePolicy) {
        return;
    }
    m_tilingRaisePolicy = tilingRaisePolicy;
    emit tilingRaisePolicyChanged();
}

void Options::setSeparateScreenFocus(bool separateScreenFocus)
{
    if (m_separateScreenFocus == separateScreenFocus) {
        return;
    }
    m_separateScreenFocus = separateScreenFocus;
    emit separateScreenFocusChanged();
}

void Options::setActiveMouseScreen(bool activeMouseScreen)
{
    if (m_activeMouseScreen == activeMouseScreen) {
        return;
    }
    m_activeMouseScreen = activeMouseScreen;
    emit activeMouseScreenChanged();
}

void Options::setPlacement(int placement)
{
    if (m_placement == static_cast<Placement::Policy>(placement)) {
        return;
    }
    m_placement = static_cast<Placement::Policy>(placement);
    emit placementChanged();
}

void Options::setBorderSnapZone(int borderSnapZone)
{
    if (m_borderSnapZone == borderSnapZone) {
        return;
    }
    m_borderSnapZone = borderSnapZone;
    emit borderSnapZoneChanged();
}

void Options::setWindowSnapZone(int windowSnapZone)
{
    if (m_windowSnapZone == windowSnapZone) {
        return;
    }
    m_windowSnapZone = windowSnapZone;
    emit windowSnapZoneChanged();
}

void Options::setCenterSnapZone(int centerSnapZone)
{
    if (m_centerSnapZone == centerSnapZone) {
        return;
    }
    m_centerSnapZone = centerSnapZone;
    emit centerSnapZoneChanged();
}

void Options::setSnapOnlyWhenOverlapping(bool snapOnlyWhenOverlapping)
{
    if (m_snapOnlyWhenOverlapping == snapOnlyWhenOverlapping) {
        return;
    }
    m_snapOnlyWhenOverlapping = snapOnlyWhenOverlapping;
    emit snapOnlyWhenOverlappingChanged();
}

void Options::setShowDesktopIsMinimizeAll(bool showDesktopIsMinimizeAll)
{
    if (m_showDesktopIsMinimizeAll == showDesktopIsMinimizeAll) {
        return;
    }
    m_showDesktopIsMinimizeAll = showDesktopIsMinimizeAll;
    emit showDesktopIsMinimizeAllChanged();
}

void Options::setRollOverDesktops(bool rollOverDesktops)
{
    if (m_rollOverDesktops == rollOverDesktops) {
        return;
    }
    m_rollOverDesktops = rollOverDesktops;
    emit rollOverDesktopsChanged();
}

void Options::setFocusStealingPreventionLevel(int focusStealingPreventionLevel)
{
    if (m_focusStealingPreventionLevel == focusStealingPreventionLevel) {
        return;
    }
    m_focusStealingPreventionLevel = focusStealingPreventionLevel;
    emit focusStealingPreventionLevelChanged();
}

void Options::setLegacyFullscreenSupport(bool legacyFullscreenSupport)
{
    if (m_legacyFullscreenSupport == legacyFullscreenSupport) {
        return;
    }
    m_legacyFullscreenSupport = legacyFullscreenSupport;
    emit legacyFullscreenSupportChanged();
}

void Options::setOperationTitlebarDblClick(WindowOperation operationTitlebarDblClick)
{
    if (OpTitlebarDblClick == operationTitlebarDblClick) {
        return;
    }
    OpTitlebarDblClick = operationTitlebarDblClick;
    emit operationTitlebarDblClickChanged();
}

void Options::setCommandActiveTitlebar1(MouseCommand commandActiveTitlebar1)
{
    if (CmdActiveTitlebar1 == commandActiveTitlebar1) {
        return;
    }
    CmdActiveTitlebar1 = commandActiveTitlebar1;
    emit commandActiveTitlebar1Changed();
}

void Options::setCommandActiveTitlebar2(MouseCommand commandActiveTitlebar2)
{
    if (CmdActiveTitlebar2 == commandActiveTitlebar2) {
        return;
    }
    CmdActiveTitlebar2 = commandActiveTitlebar2;
    emit commandActiveTitlebar2Changed();
}

void Options::setCommandActiveTitlebar3(MouseCommand commandActiveTitlebar3)
{
    if (CmdActiveTitlebar3 == commandActiveTitlebar3) {
        return;
    }
    CmdActiveTitlebar3 = commandActiveTitlebar3;
    emit commandActiveTitlebar3Changed();
}

void Options::setCommandInactiveTitlebar1(MouseCommand commandInactiveTitlebar1)
{
    if (CmdInactiveTitlebar1 == commandInactiveTitlebar1) {
        return;
    }
    CmdInactiveTitlebar1 = commandInactiveTitlebar1;
    emit commandInactiveTitlebar1Changed();
}

void Options::setCommandInactiveTitlebar2(MouseCommand commandInactiveTitlebar2)
{
    if (CmdInactiveTitlebar2 == commandInactiveTitlebar2) {
        return;
    }
    CmdInactiveTitlebar2 = commandInactiveTitlebar2;
    emit commandInactiveTitlebar2Changed();
}

void Options::setCommandInactiveTitlebar3(MouseCommand commandInactiveTitlebar3)
{
    if (CmdInactiveTitlebar3 == commandInactiveTitlebar3) {
        return;
    }
    CmdInactiveTitlebar3 = commandInactiveTitlebar3;
    emit commandInactiveTitlebar3Changed();
}

void Options::setCommandWindow1(MouseCommand commandWindow1)
{
    if (CmdWindow1 == commandWindow1) {
        return;
    }
    CmdWindow1 = commandWindow1;
    emit commandWindow1Changed();
}

void Options::setCommandWindow2(MouseCommand commandWindow2)
{
    if (CmdWindow2 == commandWindow2) {
        return;
    }
    CmdWindow2 = commandWindow2;
    emit commandWindow2Changed();
}

void Options::setCommandWindow3(MouseCommand commandWindow3)
{
    if (CmdWindow3 == commandWindow3) {
        return;
    }
    CmdWindow3 = commandWindow3;
    emit commandWindow3Changed();
}

void Options::setCommandWindowWheel(MouseCommand commandWindowWheel)
{
    if (CmdWindowWheel == commandWindowWheel) {
        return;
    }
    CmdWindowWheel = commandWindowWheel;
    emit commandWindowWheelChanged();
}

void Options::setCommandAll1(MouseCommand commandAll1)
{
    if (CmdAll1 == commandAll1) {
        return;
    }
    CmdAll1 = commandAll1;
    emit commandAll1Changed();
}

void Options::setCommandAll2(MouseCommand commandAll2)
{
    if (CmdAll2 == commandAll2) {
        return;
    }
    CmdAll2 = commandAll2;
    emit commandAll2Changed();
}

void Options::setCommandAll3(MouseCommand commandAll3)
{
    if (CmdAll3 == commandAll3) {
        return;
    }
    CmdAll3 = commandAll3;
    emit commandAll3Changed();
}

void Options::setKeyCmdAllModKey(uint keyCmdAllModKey)
{
    if (CmdAllModKey == keyCmdAllModKey) {
        return;
    }
    CmdAllModKey = keyCmdAllModKey;
    emit keyCmdAllModKeyChanged();
}

void Options::setShowGeometryTip(bool showGeometryTip)
{
    if (show_geometry_tip == showGeometryTip) {
        return;
    }
    show_geometry_tip = showGeometryTip;
    emit showGeometryTipChanged();
}

void Options::setElectricBorderDelay(int electricBorderDelay)
{
    if (electric_border_delay == electricBorderDelay) {
        return;
    }
    electric_border_delay = electricBorderDelay;
    emit electricBorderDelayChanged();
}

void Options::setElectricBorderCooldown(int electricBorderCooldown)
{
    if (electric_border_cooldown == electricBorderCooldown) {
        return;
    }
    electric_border_cooldown = electricBorderCooldown;
    emit electricBorderCooldownChanged();
}

void Options::setElectricBorderPushbackPixels(int electricBorderPushbackPixels)
{
    if (electric_border_pushback_pixels == electricBorderPushbackPixels) {
        return;
    }
    electric_border_pushback_pixels = electricBorderPushbackPixels;
    emit electricBorderPushbackPixelsChanged();
}

void Options::setElectricBorderMaximize(bool electricBorderMaximize)
{
    if (electric_border_maximize == electricBorderMaximize) {
        return;
    }
    electric_border_maximize = electricBorderMaximize;
    emit electricBorderMaximizeChanged();
}

void Options::setElectricBorderTiling(bool electricBorderTiling)
{
    if (electric_border_tiling == electricBorderTiling) {
        return;
    }
    electric_border_tiling = electricBorderTiling;
    emit electricBorderTilingChanged();
}

void Options::setBorderlessMaximizedWindows(bool borderlessMaximizedWindows)
{
    if (borderless_maximized_windows == borderlessMaximizedWindows) {
        return;
    }
    borderless_maximized_windows = borderlessMaximizedWindows;
    emit borderlessMaximizedWindowsChanged();
}

void Options::setKillPingTimeout(int killPingTimeout)
{
    if (m_killPingTimeout == killPingTimeout) {
        return;
    }
    m_killPingTimeout = killPingTimeout;
    emit killPingTimeoutChanged();
}

void Options::setHideUtilityWindowsForInactive(bool hideUtilityWindowsForInactive)
{
    if (m_hideUtilityWindowsForInactive == hideUtilityWindowsForInactive) {
        return;
    }
    m_hideUtilityWindowsForInactive = hideUtilityWindowsForInactive;
    emit hideUtilityWindowsForInactiveChanged();
}

void Options::setInactiveTabsSkipTaskbar(bool inactiveTabsSkipTaskbar)
{
    if (m_inactiveTabsSkipTaskbar == inactiveTabsSkipTaskbar) {
        return;
    }
    m_inactiveTabsSkipTaskbar = inactiveTabsSkipTaskbar;
    emit inactiveTabsSkipTaskbarChanged();
}

void Options::setAutogroupSimilarWindows(bool autogroupSimilarWindows)
{
    if (m_autogroupSimilarWindows == autogroupSimilarWindows) {
        return;
    }
    m_autogroupSimilarWindows = autogroupSimilarWindows;
    emit autogroupSimilarWindowsChanged();
}

void Options::setAutogroupInForeground(bool autogroupInForeground)
{
    if (m_autogroupInForeground == autogroupInForeground) {
        return;
    }
    m_autogroupInForeground = autogroupInForeground;
    emit autogroupInForegroundChanged();
}

void Options::setCompositingMode(int compositingMode)
{
    if (m_compositingMode == static_cast<CompositingType>(compositingMode)) {
        return;
    }
    m_compositingMode = static_cast<CompositingType>(compositingMode);
    emit compositingModeChanged();
}

void Options::setUseCompositing(bool useCompositing)
{
    if (m_useCompositing == useCompositing) {
        return;
    }
    m_useCompositing = useCompositing;
    emit useCompositingChanged();
}

void Options::setCompositingInitialized(bool compositingInitialized)
{
    if (m_compositingInitialized == compositingInitialized) {
        return;
    }
    m_compositingInitialized = compositingInitialized;
    emit compositingInitializedChanged();
}

void Options::setHiddenPreviews(int hiddenPreviews)
{
    if (m_hiddenPreviews == static_cast<HiddenPreviews>(hiddenPreviews)) {
        return;
    }
    m_hiddenPreviews = static_cast<HiddenPreviews>(hiddenPreviews);
    emit hiddenPreviewsChanged();
}

void Options::setUnredirectFullscreen(bool unredirectFullscreen)
{
    if (m_unredirectFullscreen == unredirectFullscreen) {
        return;
    }
    m_unredirectFullscreen = unredirectFullscreen;
    emit unredirectFullscreenChanged();
}

void Options::setGlSmoothScale(int glSmoothScale)
{
    if (m_glSmoothScale == glSmoothScale) {
        return;
    }
    m_glSmoothScale = glSmoothScale;
    emit glSmoothScaleChanged();
}

void Options::setGlVSync(bool glVSync)
{
    if (m_glVSync == glVSync) {
        return;
    }
    m_glVSync = glVSync;
    emit glVSyncChanged();
}

void Options::setXrenderSmoothScale(bool xrenderSmoothScale)
{
    if (m_xrenderSmoothScale == xrenderSmoothScale) {
        return;
    }
    m_xrenderSmoothScale = xrenderSmoothScale;
    emit xrenderSmoothScaleChanged();
}

void Options::setMaxFpsInterval(uint maxFpsInterval)
{
    if (m_maxFpsInterval == maxFpsInterval) {
        return;
    }
    m_maxFpsInterval = maxFpsInterval;
    emit maxFpsIntervalChanged();
}

void Options::setRefreshRate(uint refreshRate)
{
    if (m_refreshRate == refreshRate) {
        return;
    }
    m_refreshRate = refreshRate;
    emit refreshRateChanged();
}

void Options::setGlDirect(bool glDirect)
{
    if (m_glDirect == glDirect) {
        return;
    }
    m_glDirect = glDirect;
    emit glDirectChanged();
}

void Options::setGlStrictBinding(bool glStrictBinding)
{
    if (m_glStrictBinding == glStrictBinding) {
        return;
    }
    m_glStrictBinding = glStrictBinding;
    emit glStrictBindingChanged();
}

void Options::setElectricBorders(int borders)
{
    if (electric_borders == borders) {
        return;
    }
    electric_borders = borders;
    emit electricBordersChanged();
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
    m_focusPolicy = ClickToFocus; // what a default :-)
    if (val == "FocusFollowsMouse")
        m_focusPolicy = FocusFollowsMouse;
    else if (val == "FocusUnderMouse")
        m_focusPolicy = FocusUnderMouse;
    else if (val == "FocusStrictlyUnderMouse")
        m_focusPolicy = FocusStrictlyUnderMouse;

    m_nextFocusPrefersMouse = config.readEntry("NextFocusPrefersMouse", false);

    m_separateScreenFocus = config.readEntry("SeparateScreenFocus", false);
    m_activeMouseScreen = config.readEntry("ActiveMouseScreen", m_focusPolicy != ClickToFocus);

    m_rollOverDesktops = config.readEntry("RollOverDesktops", true);

    m_legacyFullscreenSupport = config.readEntry("LegacyFullscreenSupport", false);

//    focusStealingPreventionLevel = config.readEntry( "FocusStealingPreventionLevel", 2 );
    // TODO use low level for now
    m_focusStealingPreventionLevel = config.readEntry("FocusStealingPreventionLevel", 1);
    m_focusStealingPreventionLevel = qMax(0, qMin(4, m_focusStealingPreventionLevel));
    if (!focusPolicyIsReasonable())  // #48786, comments #7 and later
        m_focusStealingPreventionLevel = 0;

#ifdef KWIN_BUILD_DECORATIONS
    m_placement = Placement::policyFromString(config.readEntry("Placement"), true);
#else
    m_placement = Placement::Maximizing;
#endif

    if (m_focusPolicy == ClickToFocus) {
        m_autoRaise = false;
        m_autoRaiseInterval = 0;
        m_delayFocusInterval = 0;
    } else {
        m_autoRaise = config.readEntry("AutoRaise", false);
        m_autoRaiseInterval = config.readEntry("AutoRaiseInterval", 0);
        m_delayFocusInterval = config.readEntry("DelayFocusInterval", 0);
    }

    m_shadeHover = config.readEntry("ShadeHover", false);
    m_shadeHoverInterval = config.readEntry("ShadeHoverInterval", 250);

    m_tilingOn = config.readEntry("TilingOn", false);
    m_tilingLayout = static_cast<TilingLayoutFactory::Layouts>(config.readEntry("TilingDefaultLayout", 0));
    m_tilingRaisePolicy = config.readEntry("TilingRaisePolicy", 0);

    // important: autoRaise implies ClickRaise
    m_clickRaise = m_autoRaise || config.readEntry("ClickRaise", true);

    m_borderSnapZone = config.readEntry("BorderSnapZone", 10);
    m_windowSnapZone = config.readEntry("WindowSnapZone", 10);
    m_centerSnapZone = config.readEntry("CenterSnapZone", 0);
    m_snapOnlyWhenOverlapping = config.readEntry("SnapOnlyWhenOverlapping", false);

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

    m_killPingTimeout = config.readEntry("KillPingTimeout", 5000);
    m_hideUtilityWindowsForInactive = config.readEntry("HideUtilityWindowsForInactive", true);
    m_inactiveTabsSkipTaskbar = config.readEntry("InactiveTabsSkipTaskbar", false);
    m_autogroupSimilarWindows = config.readEntry("AutogroupSimilarWindows", false);
    m_autogroupInForeground = config.readEntry("AutogroupInForeground", true);
    m_showDesktopIsMinimizeAll = config.readEntry("ShowDesktopIsMinimizeAll", false);

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
    m_maxFpsInterval = qRound(1000.0 / config.readEntry("MaxFPS", 60));
    m_refreshRate = config.readEntry("RefreshRate", 0);

    // Read button tooltip animation effect from kdeglobals
    // Since we want to allow users to enable window decoration tooltips
    // and not kstyle tooltips and vise-versa, we don't read the
    // "EffectNoTooltip" setting from kdeglobals.


//    QToolTip::setGloballyEnabled( d->show_tooltips );
// KDE4 this probably needs to be done manually in clients

    // Driver-specific config detection
    m_compositingInitialized = false;
    reloadCompositingSettings();

    emit configChanged();

    return changed;
}

void Options::reloadCompositingSettings(bool force)
{
    KSharedConfig::Ptr _config = KGlobal::config();
    KConfigGroup config(_config, "Compositing");

    QString compositingBackend = config.readEntry("Backend", "OpenGL");
    if (compositingBackend == "XRender")
        m_compositingMode = XRenderCompositing;
    else
        m_compositingMode = OpenGLCompositing;

    m_useCompositing = false;
    if (const char *c = getenv("KWIN_COMPOSE")) {
        switch(c[0]) {
        case 'O':
            kDebug(1212) << "Compositing forced to OpenGL mode by environment variable";
            m_compositingMode = OpenGLCompositing;
            m_useCompositing = true;
            break;
        case 'X':
            kDebug(1212) << "Compositing forced to XRender mode by environment variable";
            m_compositingMode = XRenderCompositing;
            m_useCompositing = true;
            break;
        case 'N':
            if (getenv("KDE_FAILSAFE"))
                kDebug(1212) << "Compositing disabled forcefully by KDE failsafe mode";
            else
                kDebug(1212) << "Compositing disabled forcefully by environment variable";
            m_compositingMode = NoCompositing;
            break;
        default:
            kDebug(1212) << "Unknown KWIN_COMPOSE mode set, ignoring";
            break;
        }
    }

    if (m_compositingMode == NoCompositing)
        return; // do not even detect compositing preferences if explicitly disabled

    // it's either enforced by env or by initial resume from "suspend" or we check the settings
    m_useCompositing = m_useCompositing || force || config.readEntry("Enabled", true);

    if (!m_useCompositing)
        return; // not enforced or necessary and not "enabled" by setting

    // from now on we've an initial setup and don't have to reload settings on compositing activation
    // see Workspace::setupCompositing(), composite.cpp
    m_compositingInitialized = true;

    // Compositing settings
    CompositingPrefs prefs;
    prefs.detect();

    m_useCompositing = config.readEntry("Enabled" , prefs.recommendCompositing());
    m_glDirect = prefs.enableDirectRendering();
    m_glVSync = config.readEntry("GLVSync", prefs.enableVSync());
    m_glSmoothScale = qBound(-1, config.readEntry("GLTextureFilter", 2), 2);
    m_glStrictBinding = config.readEntry("GLStrictBinding", prefs.strictBinding());

    m_xrenderSmoothScale = config.readEntry("XRenderSmoothScale", false);

    m_hiddenPreviews = HiddenPreviewsShown;
    // 4 - off, 5 - shown, 6 - always, other are old values
    int hps = config.readEntry("HiddenPreviews", 5);
    if (hps == 4)
        m_hiddenPreviews = HiddenPreviewsNever;
    else if (hps == 5)
        m_hiddenPreviews = HiddenPreviewsShown;
    else if (hps == 6)
        m_hiddenPreviews = HiddenPreviewsAlways;

    m_unredirectFullscreen = config.readEntry("UnredirectFullscreen", false);
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
    if (lowerName == "start window tab drag") return MouseDragTab;
    if (lowerName == "close") return MouseClose;
    if (lowerName == "increase opacity") return MouseOpacityMore;
    if (lowerName == "decrease opacity") return MouseOpacityLess;
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
    if (lowerName == "switch to window tab to the left/right") return MouseWheelChangeCurrentTab;
    if (lowerName == "nothing") return MouseWheelNothing;
    return MouseWheelChangeCurrentTab;
}

bool Options::showGeometryTip() const
{
    return show_geometry_tip;
}

ElectricBorderAction Options::electricBorderAction(ElectricBorder edge) const
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

int Options::electricBorders() const
{
    return electric_borders;
}

int Options::electricBorderDelay() const
{
    return electric_border_delay;
}

int Options::electricBorderCooldown() const
{
    return electric_border_cooldown;
}

Options::MouseCommand Options::wheelToMouseCommand(MouseWheelCommand com, int delta) const
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
    case MouseWheelChangeCurrentTab:
        return delta > 0 ? MousePreviousTab : MouseNextTab;
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
