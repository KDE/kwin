/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <m.graesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "options.h"
#include "config-kwin.h"
#include "utils.h"
#include "platform.h"

#ifndef KCMRULES

#include <QProcess>

#include "screens.h"
#include "settings.h"
#include <kwinglplatform.h>
#include <QOpenGLContext>

#endif //KCMRULES

namespace KWin
{

#ifndef KCMRULES

int currentRefreshRate()
{
    return Options::currentRefreshRate();
}

int Options::currentRefreshRate()
{
    int rate = -1;
    QString syncScreenName(QLatin1String("primary screen"));
    if (options->refreshRate() > 0) {  // use manually configured refresh rate
        rate = options->refreshRate();
    } else if (Screens::self()->count() > 0) {
        // prefer the refreshrate calculated from the screens mode information
        // at least the nvidia driver reports 50Hz BS ... *again*!
        int syncScreen = 0;
        if (Screens::self()->count() > 1) {
            const QByteArray syncDisplayDevice(qgetenv("__GL_SYNC_DISPLAY_DEVICE"));
            // if __GL_SYNC_DISPLAY_DEVICE is exported, the GPU shall sync to that device
            // so we try to use its refresh rate
            if (!syncDisplayDevice.isEmpty()) {
                for (int i = 0; i < Screens::self()->count(); ++i) {
                    if (Screens::self()->name(i) == syncDisplayDevice) {
                        syncScreenName = Screens::self()->name(i);
                        syncScreen = i;
                        break;
                    }
                }
            }
        }
        rate = qRound(Screens::self()->refreshRate(syncScreen)); // TODO forward float precision?
    }

    // 0Hz or less is invalid, so we fallback to a default rate
    if (rate <= 0)
        rate = 60; // and not shitty 50Hz for sure! *grrr*

    // QTimer gives us 1msec (1000Hz) at best, so we ignore anything higher;
    // however, additional throttling prevents very high rates from taking place anyway
    else if (rate > 1000)
        rate = 1000;
    qCDebug(KWIN_CORE) << "Vertical Refresh rate " << rate << "Hz (" << syncScreenName << ")";
    return rate;
}

Options::Options(QObject *parent)
    : QObject(parent)
    , m_settings(new Settings(kwinApp()->config()))
    , m_focusPolicy(ClickToFocus)
    , m_nextFocusPrefersMouse(false)
    , m_clickRaise(false)
    , m_autoRaise(false)
    , m_autoRaiseInterval(0)
    , m_delayFocusInterval(0)
    , m_shadeHover(false)
    , m_shadeHoverInterval(0)
    , m_separateScreenFocus(false)
    , m_placement(Placement::NoPlacement)
    , m_borderSnapZone(0)
    , m_windowSnapZone(0)
    , m_centerSnapZone(0)
    , m_snapOnlyWhenOverlapping(false)
    , m_rollOverDesktops(false)
    , m_focusStealingPreventionLevel(0)
    , m_killPingTimeout(0)
    , m_hideUtilityWindowsForInactive(false)
    , m_xwaylandCrashPolicy(Options::defaultXwaylandCrashPolicy())
    , m_xwaylandMaxCrashCount(Options::defaultXwaylandMaxCrashCount())
    , m_compositingMode(Options::defaultCompositingMode())
    , m_useCompositing(Options::defaultUseCompositing())
    , m_hiddenPreviews(Options::defaultHiddenPreviews())
    , m_glSmoothScale(Options::defaultGlSmoothScale())
    , m_xrenderSmoothScale(Options::defaultXrenderSmoothScale())
    , m_maxFpsInterval(Options::defaultMaxFpsInterval())
    , m_refreshRate(Options::defaultRefreshRate())
    , m_vBlankTime(Options::defaultVBlankTime())
    , m_glStrictBinding(Options::defaultGlStrictBinding())
    , m_glStrictBindingFollowsDriver(Options::defaultGlStrictBindingFollowsDriver())
    , m_glCoreProfile(Options::defaultGLCoreProfile())
    , m_glPreferBufferSwap(Options::defaultGlPreferBufferSwap())
    , m_glPlatformInterface(Options::defaultGlPlatformInterface())
    , m_windowsBlockCompositing(true)
    , OpTitlebarDblClick(Options::defaultOperationTitlebarDblClick())
    , CmdActiveTitlebar1(Options::defaultCommandActiveTitlebar1())
    , CmdActiveTitlebar2(Options::defaultCommandActiveTitlebar2())
    , CmdActiveTitlebar3(Options::defaultCommandActiveTitlebar3())
    , CmdInactiveTitlebar1(Options::defaultCommandInactiveTitlebar1())
    , CmdInactiveTitlebar2(Options::defaultCommandInactiveTitlebar2())
    , CmdInactiveTitlebar3(Options::defaultCommandInactiveTitlebar3())
    , CmdTitlebarWheel(Options::defaultCommandTitlebarWheel())
    , CmdWindow1(Options::defaultCommandWindow1())
    , CmdWindow2(Options::defaultCommandWindow2())
    , CmdWindow3(Options::defaultCommandWindow3())
    , CmdWindowWheel(Options::defaultCommandWindowWheel())
    , CmdAll1(Options::defaultCommandAll1())
    , CmdAll2(Options::defaultCommandAll2())
    , CmdAll3(Options::defaultCommandAll3())
    , CmdAllWheel(Options::defaultCommandAllWheel())
    , CmdAllModKey(Options::defaultKeyCmdAllModKey())
    , electric_border_maximize(false)
    , electric_border_tiling(false)
    , electric_border_corner_ratio(0.0)
    , borderless_maximized_windows(false)
    , show_geometry_tip(false)
    , condensed_title(false)
{
    m_settings->setDefaults();
    syncFromKcfgc();

    m_configWatcher = KConfigWatcher::create(m_settings->sharedConfig());
    connect(m_configWatcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group, const QByteArrayList &names) {
        if (group.name() == QLatin1String("KDE") && names.contains(QByteArrayLiteral("AnimationDurationFactor"))) {
            emit animationSpeedChanged();
        }
    });
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
    if (m_focusPolicy == ClickToFocus) {
        setAutoRaise(false);
        setAutoRaiseInterval(0);
        setDelayFocusInterval(0);
    }
}

void Options::setNextFocusPrefersMouse(bool nextFocusPrefersMouse)
{
    if (m_nextFocusPrefersMouse == nextFocusPrefersMouse) {
        return;
    }
    m_nextFocusPrefersMouse = nextFocusPrefersMouse;
    emit nextFocusPrefersMouseChanged();
}

void Options::setXwaylandCrashPolicy(XwaylandCrashPolicy crashPolicy)
{
    if (m_xwaylandCrashPolicy == crashPolicy) {
        return;
    }
    m_xwaylandCrashPolicy = crashPolicy;
    emit xwaylandCrashPolicyChanged();
}

void Options::setXwaylandMaxCrashCount(int maxCrashCount)
{
    if (m_xwaylandMaxCrashCount == maxCrashCount) {
        return;
    }
    m_xwaylandMaxCrashCount = maxCrashCount;
    emit xwaylandMaxCrashCountChanged();
}

void Options::setClickRaise(bool clickRaise)
{
    if (m_autoRaise) {
        // important: autoRaise implies ClickRaise
        clickRaise = true;
    }
    if (m_clickRaise == clickRaise) {
        return;
    }
    m_clickRaise = clickRaise;
    emit clickRaiseChanged();
}

void Options::setAutoRaise(bool autoRaise)
{
    if (m_focusPolicy == ClickToFocus) {
        autoRaise = false;
    }
    if (m_autoRaise == autoRaise) {
        return;
    }
    m_autoRaise = autoRaise;
    if (m_autoRaise) {
        // important: autoRaise implies ClickRaise
        setClickRaise(true);
    }
    emit autoRaiseChanged();
}

void Options::setAutoRaiseInterval(int autoRaiseInterval)
{
    if (m_focusPolicy == ClickToFocus) {
        autoRaiseInterval = 0;
    }
    if (m_autoRaiseInterval == autoRaiseInterval) {
        return;
    }
    m_autoRaiseInterval = autoRaiseInterval;
    emit autoRaiseIntervalChanged();
}

void Options::setDelayFocusInterval(int delayFocusInterval)
{
    if (m_focusPolicy == ClickToFocus) {
        delayFocusInterval = 0;
    }
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

void Options::setSeparateScreenFocus(bool separateScreenFocus)
{
    if (m_separateScreenFocus == separateScreenFocus) {
        return;
    }
    m_separateScreenFocus = separateScreenFocus;
    emit separateScreenFocusChanged(m_separateScreenFocus);
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

void Options::setRollOverDesktops(bool rollOverDesktops)
{
    if (m_rollOverDesktops == rollOverDesktops) {
        return;
    }
    m_rollOverDesktops = rollOverDesktops;
    emit rollOverDesktopsChanged(m_rollOverDesktops);
}

void Options::setFocusStealingPreventionLevel(int focusStealingPreventionLevel)
{
    if (!focusPolicyIsReasonable()) {
        focusStealingPreventionLevel = 0;
    }
    if (m_focusStealingPreventionLevel == focusStealingPreventionLevel) {
        return;
    }
    m_focusStealingPreventionLevel = qMax(0, qMin(4, focusStealingPreventionLevel));
    emit focusStealingPreventionLevelChanged();
}

void Options::setOperationTitlebarDblClick(WindowOperation operationTitlebarDblClick)
{
    if (OpTitlebarDblClick == operationTitlebarDblClick) {
        return;
    }
    OpTitlebarDblClick = operationTitlebarDblClick;
    emit operationTitlebarDblClickChanged();
}

void Options::setOperationMaxButtonLeftClick(WindowOperation op)
{
    if (opMaxButtonLeftClick == op) {
        return;
    }
    opMaxButtonLeftClick = op;
    emit operationMaxButtonLeftClickChanged();
}

void Options::setOperationMaxButtonRightClick(WindowOperation op)
{
    if (opMaxButtonRightClick == op) {
        return;
    }
    opMaxButtonRightClick = op;
    emit operationMaxButtonRightClickChanged();
}

void Options::setOperationMaxButtonMiddleClick(WindowOperation op)
{
    if (opMaxButtonMiddleClick == op) {
        return;
    }
    opMaxButtonMiddleClick = op;
    emit operationMaxButtonMiddleClickChanged();
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

void Options::setCondensedTitle(bool condensedTitle)
{
    if (condensed_title == condensedTitle) {
        return;
    }
    condensed_title = condensedTitle;
    emit condensedTitleChanged();
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

void Options::setElectricBorderCornerRatio(float electricBorderCornerRatio)
{
    if (electric_border_corner_ratio == electricBorderCornerRatio) {
        return;
    }
    electric_border_corner_ratio = electricBorderCornerRatio;
    emit electricBorderCornerRatioChanged();
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

void Options::setHiddenPreviews(int hiddenPreviews)
{
    if (m_hiddenPreviews == static_cast<HiddenPreviews>(hiddenPreviews)) {
        return;
    }
    m_hiddenPreviews = static_cast<HiddenPreviews>(hiddenPreviews);
    emit hiddenPreviewsChanged();
}

void Options::setGlSmoothScale(int glSmoothScale)
{
    if (m_glSmoothScale == glSmoothScale) {
        return;
    }
    m_glSmoothScale = glSmoothScale;
    emit glSmoothScaleChanged();
}

void Options::setXrenderSmoothScale(bool xrenderSmoothScale)
{
    if (m_xrenderSmoothScale == xrenderSmoothScale) {
        return;
    }
    m_xrenderSmoothScale = xrenderSmoothScale;
    emit xrenderSmoothScaleChanged();
}

void Options::setMaxFpsInterval(qint64 maxFpsInterval)
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

void Options::setVBlankTime(qint64 vBlankTime)
{
    if (m_vBlankTime == vBlankTime) {
        return;
    }
    m_vBlankTime = vBlankTime;
    emit vBlankTimeChanged();
}

void Options::setGlStrictBinding(bool glStrictBinding)
{
    if (m_glStrictBinding == glStrictBinding) {
        return;
    }
    m_glStrictBinding = glStrictBinding;
    emit glStrictBindingChanged();
}

void Options::setGlStrictBindingFollowsDriver(bool glStrictBindingFollowsDriver)
{
    if (m_glStrictBindingFollowsDriver == glStrictBindingFollowsDriver) {
        return;
    }
    m_glStrictBindingFollowsDriver = glStrictBindingFollowsDriver;
    emit glStrictBindingFollowsDriverChanged();
}

void Options::setGLCoreProfile(bool value)
{
    if (m_glCoreProfile == value) {
        return;
    }
    m_glCoreProfile = value;
    emit glCoreProfileChanged();
}

void Options::setWindowsBlockCompositing(bool value)
{
    if (m_windowsBlockCompositing == value) {
        return;
    }
    m_windowsBlockCompositing = value;
    emit windowsBlockCompositingChanged();
}

void Options::setGlPreferBufferSwap(char glPreferBufferSwap)
{
    if (glPreferBufferSwap == 'a') {
        // buffer copying is very fast with the nvidia blob
        // but due to restrictions in DRI2 *incredibly* slow for all MESA drivers
        // see https://www.x.org/releases/X11R7.7/doc/dri2proto/dri2proto.txt, item 2.5
        if (GLPlatform::instance()->driver() == Driver_NVidia)
            glPreferBufferSwap = CopyFrontBuffer;
        else if (GLPlatform::instance()->driver() != Driver_Unknown) // undetected, finally resolved when context is initialized
            glPreferBufferSwap = ExtendDamage;
    }
    if (m_glPreferBufferSwap == (GlSwapStrategy)glPreferBufferSwap) {
        return;
    }
    m_glPreferBufferSwap = (GlSwapStrategy)glPreferBufferSwap;
    emit glPreferBufferSwapChanged();
}

void Options::setGlPlatformInterface(OpenGLPlatformInterface interface)
{
    // check environment variable
    const QByteArray envOpenGLInterface(qgetenv("KWIN_OPENGL_INTERFACE"));
    if (!envOpenGLInterface.isEmpty()) {
        if (qstrcmp(envOpenGLInterface, "egl") == 0) {
            qCDebug(KWIN_CORE) << "Forcing EGL native interface through environment variable";
            interface = EglPlatformInterface;
        } else if (qstrcmp(envOpenGLInterface, "glx") == 0) {
            qCDebug(KWIN_CORE) << "Forcing GLX native interface through environment variable";
            interface = GlxPlatformInterface;
        }
    }
    if (kwinApp()->shouldUseWaylandForCompositing() && interface == GlxPlatformInterface) {
        // Glx is impossible on Wayland, enforce egl
        qCDebug(KWIN_CORE) << "Forcing EGL native interface for Wayland mode";
        interface = EglPlatformInterface;
    }
#if !HAVE_EPOXY_GLX
    qCDebug(KWIN_CORE) << "Forcing EGL native interface as compiled without GLX support";
    interface = EglPlatformInterface;
#endif
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES) {
        qCDebug(KWIN_CORE) << "Forcing EGL native interface as Qt uses OpenGL ES";
        interface = EglPlatformInterface;
    } else if (qstrcmp(qgetenv("KWIN_COMPOSE"), "O2ES") == 0) {
        qCDebug(KWIN_CORE) << "Forcing EGL native interface as OpenGL ES requested through KWIN_COMPOSE environment variable.";
        interface = EglPlatformInterface;
    }

    if (m_glPlatformInterface == interface) {
        return;
    }
    m_glPlatformInterface = interface;
    emit glPlatformInterfaceChanged();
}

void Options::reparseConfiguration()
{
    m_settings->config()->reparseConfiguration();
}

void Options::updateSettings()
{
    loadConfig();
    // Read button tooltip animation effect from kdeglobals
    // Since we want to allow users to enable window decoration tooltips
    // and not kstyle tooltips and vise-versa, we don't read the
    // "EffectNoTooltip" setting from kdeglobals.


//    QToolTip::setGloballyEnabled( d->show_tooltips );
// KDE4 this probably needs to be done manually in clients

    // Driver-specific config detection
    reloadCompositingSettings();

    emit configChanged();
}

void Options::loadConfig()
{
    m_settings->load();

    syncFromKcfgc();

    // Electric borders
    KConfigGroup config(m_settings->config(), "Windows");
    OpTitlebarDblClick = windowOperation(config.readEntry("TitlebarDoubleClickCommand", "Maximize"), true);
    setOperationMaxButtonLeftClick(windowOperation(config.readEntry("MaximizeButtonLeftClickCommand", "Maximize"), true));
    setOperationMaxButtonMiddleClick(windowOperation(config.readEntry("MaximizeButtonMiddleClickCommand", "Maximize (vertical only)"), true));
    setOperationMaxButtonRightClick(windowOperation(config.readEntry("MaximizeButtonRightClickCommand", "Maximize (horizontal only)"), true));

    // Mouse bindings
    config = KConfigGroup(m_settings->config(), "MouseBindings");
    // TODO: add properties for missing options
    CmdTitlebarWheel = mouseWheelCommand(config.readEntry("CommandTitlebarWheel", "Nothing"));
    CmdAllModKey = (config.readEntry("CommandAllKey", "Meta") == QStringLiteral("Meta")) ? Qt::Key_Meta : Qt::Key_Alt;
    CmdAllWheel = mouseWheelCommand(config.readEntry("CommandAllWheel", "Nothing"));
    setCommandActiveTitlebar1(mouseCommand(config.readEntry("CommandActiveTitlebar1", "Raise"), true));
    setCommandActiveTitlebar2(mouseCommand(config.readEntry("CommandActiveTitlebar2", "Nothing"), true));
    setCommandActiveTitlebar3(mouseCommand(config.readEntry("CommandActiveTitlebar3", "Operations menu"), true));
    setCommandInactiveTitlebar1(mouseCommand(config.readEntry("CommandInactiveTitlebar1", "Activate and raise"), true));
    setCommandInactiveTitlebar2(mouseCommand(config.readEntry("CommandInactiveTitlebar2", "Nothing"), true));
    setCommandInactiveTitlebar3(mouseCommand(config.readEntry("CommandInactiveTitlebar3", "Operations menu"), true));
    setCommandWindow1(mouseCommand(config.readEntry("CommandWindow1", "Activate, raise and pass click"), false));
    setCommandWindow2(mouseCommand(config.readEntry("CommandWindow2", "Activate and pass click"), false));
    setCommandWindow3(mouseCommand(config.readEntry("CommandWindow3", "Activate and pass click"), false));
    setCommandWindowWheel(mouseCommand(config.readEntry("CommandWindowWheel", "Scroll"), false));
    setCommandAll1(mouseCommand(config.readEntry("CommandAll1", "Move"), false));
    setCommandAll2(mouseCommand(config.readEntry("CommandAll2", "Toggle raise and lower"), false));
    setCommandAll3(mouseCommand(config.readEntry("CommandAll3", "Resize"), false));

    // TODO: should they be moved into reloadCompositingSettings?
    config = KConfigGroup(m_settings->config(), "Compositing");
    setMaxFpsInterval(1 * 1000 * 1000 * 1000 / config.readEntry("MaxFPS", Options::defaultMaxFps()));
    setRefreshRate(config.readEntry("RefreshRate", Options::defaultRefreshRate()));
    setVBlankTime(config.readEntry("VBlankTime", Options::defaultVBlankTime()) * 1000); // config in micro, value in nano resolution

    // Modifier Only Shortcuts
    config = KConfigGroup(m_settings->config(), "ModifierOnlyShortcuts");
    m_modifierOnlyShortcuts.clear();
    if (config.hasKey("Shift")) {
        m_modifierOnlyShortcuts.insert(Qt::ShiftModifier, config.readEntry("Shift", QStringList()));
    }
    if (config.hasKey("Control")) {
        m_modifierOnlyShortcuts.insert(Qt::ControlModifier, config.readEntry("Control", QStringList()));
    }
    if (config.hasKey("Alt")) {
        m_modifierOnlyShortcuts.insert(Qt::AltModifier, config.readEntry("Alt", QStringList()));
    }
    m_modifierOnlyShortcuts.insert(Qt::MetaModifier, config.readEntry("Meta", QStringList{QStringLiteral("org.kde.plasmashell"),
                                                                                          QStringLiteral("/PlasmaShell"),
                                                                                          QStringLiteral("org.kde.PlasmaShell"),
                                                                                          QStringLiteral("activateLauncherMenu")}));
}

void Options::syncFromKcfgc()
{
    setShowGeometryTip(m_settings->geometryTip());
    setCondensedTitle(m_settings->condensedTitle());
    setFocusPolicy(m_settings->focusPolicy());
    setNextFocusPrefersMouse(m_settings->nextFocusPrefersMouse());
    setSeparateScreenFocus(m_settings->separateScreenFocus());
    setRollOverDesktops(m_settings->rollOverDesktops());
    setFocusStealingPreventionLevel(m_settings->focusStealingPreventionLevel());
    setXwaylandCrashPolicy(m_settings->xwaylandCrashPolicy());
    setXwaylandMaxCrashCount(m_settings->xwaylandMaxCrashCount());

#ifdef KWIN_BUILD_DECORATIONS
    setPlacement(m_settings->placement());
#else
    setPlacement(Placement::Maximizing);
#endif

    setAutoRaise(m_settings->autoRaise());
    setAutoRaiseInterval(m_settings->autoRaiseInterval());
    setDelayFocusInterval(m_settings->delayFocusInterval());
    setShadeHover(m_settings->shadeHover());
    setShadeHoverInterval(m_settings->shadeHoverInterval());
    setClickRaise(m_settings->clickRaise());
    setBorderSnapZone(m_settings->borderSnapZone());
    setWindowSnapZone(m_settings->windowSnapZone());
    setCenterSnapZone(m_settings->centerSnapZone());
    setSnapOnlyWhenOverlapping(m_settings->snapOnlyWhenOverlapping());
    setKillPingTimeout(m_settings->killPingTimeout());
    setHideUtilityWindowsForInactive(m_settings->hideUtilityWindowsForInactive());
    setBorderlessMaximizedWindows(m_settings->borderlessMaximizedWindows());
    setElectricBorderMaximize(m_settings->electricBorderMaximize());
    setElectricBorderTiling(m_settings->electricBorderTiling());
    setElectricBorderCornerRatio(m_settings->electricBorderCornerRatio());
    setWindowsBlockCompositing(m_settings->windowsBlockCompositing());

}

bool Options::loadCompositingConfig (bool force)
{
    KConfigGroup config(m_settings->config(), "Compositing");

    bool useCompositing = false;
    CompositingType compositingMode = NoCompositing;
    QString compositingBackend = config.readEntry("Backend", "OpenGL");
    if (compositingBackend == QStringLiteral("XRender"))
        compositingMode = XRenderCompositing;
    else if (compositingBackend == "QPainter")
        compositingMode = QPainterCompositing;
    else
        compositingMode = OpenGLCompositing;

    if (const char *c = getenv("KWIN_COMPOSE")) {
        switch(c[0]) {
        case 'O':
            qCDebug(KWIN_CORE) << "Compositing forced to OpenGL mode by environment variable";
            compositingMode = OpenGLCompositing;
            useCompositing = true;
            break;
        case 'X':
            qCDebug(KWIN_CORE) << "Compositing forced to XRender mode by environment variable";
            compositingMode = XRenderCompositing;
            useCompositing = true;
            break;
        case 'Q':
            qCDebug(KWIN_CORE) << "Compositing forced to QPainter mode by environment variable";
            compositingMode = QPainterCompositing;
            useCompositing = true;
            break;
        case 'N':
            if (getenv("KDE_FAILSAFE"))
                qCDebug(KWIN_CORE) << "Compositing disabled forcefully by KDE failsafe mode";
            else
                qCDebug(KWIN_CORE) << "Compositing disabled forcefully by environment variable";
            compositingMode = NoCompositing;
            break;
        default:
            qCDebug(KWIN_CORE) << "Unknown KWIN_COMPOSE mode set, ignoring";
            break;
        }
    }
    setCompositingMode(compositingMode);

    const bool platformSupportsNoCompositing = kwinApp()->platform()->supportedCompositors().contains(NoCompositing);
    if (m_compositingMode == NoCompositing && platformSupportsNoCompositing) {
        setUseCompositing(false);
        return false; // do not even detect compositing preferences if explicitly disabled
    }

    // it's either enforced by env or by initial resume from "suspend" or we check the settings
    setUseCompositing(useCompositing || force || config.readEntry("Enabled", Options::defaultUseCompositing() || !platformSupportsNoCompositing));

    if (!m_useCompositing)
        return false; // not enforced or necessary and not "enabled" by settings
    return true;
}

void Options::reloadCompositingSettings(bool force)
{
    if (!loadCompositingConfig(force)) {
        return;
    }
    m_settings->load();
    syncFromKcfgc();

    // Compositing settings
    KConfigGroup config(m_settings->config(), "Compositing");

    setGlSmoothScale(qBound(-1, config.readEntry("GLTextureFilter", Options::defaultGlSmoothScale()), 2));
    setGlStrictBindingFollowsDriver(!config.hasKey("GLStrictBinding"));
    if (!isGlStrictBindingFollowsDriver()) {
        setGlStrictBinding(config.readEntry("GLStrictBinding", Options::defaultGlStrictBinding()));
    }
    setGLCoreProfile(config.readEntry("GLCore", Options::defaultGLCoreProfile()));

    char c = 0;
    const QString s = config.readEntry("GLPreferBufferSwap", QString(Options::defaultGlPreferBufferSwap()));
    if (!s.isEmpty())
        c = s.at(0).toLatin1();
    if (c != 'a' && c != 'c' && c != 'p' && c != 'e')
        c = 0;
    setGlPreferBufferSwap(c);

    m_xrenderSmoothScale = config.readEntry("XRenderSmoothScale", false);

    HiddenPreviews previews = Options::defaultHiddenPreviews();
    // 4 - off, 5 - shown, 6 - always, other are old values
    int hps = config.readEntry("HiddenPreviews", 5);
    if (hps == 4)
        previews = HiddenPreviewsNever;
    else if (hps == 5)
        previews = HiddenPreviewsShown;
    else if (hps == 6)
        previews = HiddenPreviewsAlways;
    setHiddenPreviews(previews);

    auto interfaceToKey = [](OpenGLPlatformInterface interface) {
        switch (interface) {
        case GlxPlatformInterface:
            return QStringLiteral("glx");
        case EglPlatformInterface:
            return QStringLiteral("egl");
        default:
            return QString();
        }
    };
    auto keyToInterface = [](const QString &key) {
        if (key == QStringLiteral("glx")) {
            return GlxPlatformInterface;
        } else if (key == QStringLiteral("egl")) {
            return EglPlatformInterface;
        }
        return defaultGlPlatformInterface();
    };
    setGlPlatformInterface(keyToInterface(config.readEntry("GLPlatformInterface", interfaceToKey(m_glPlatformInterface))));
}

// restricted should be true for operations that the user may not be able to repeat
// if the window is moved out of the workspace (e.g. if the user moves a window
// by the titlebar, and moves it too high beneath Kicker at the top edge, they
// may not be able to move it back, unless they know about Meta+LMB)
Options::WindowOperation Options::windowOperation(const QString &name, bool restricted)
{
    if (name == QStringLiteral("Move"))
        return restricted ? MoveOp : UnrestrictedMoveOp;
    else if (name == QStringLiteral("Resize"))
        return restricted ? ResizeOp : UnrestrictedResizeOp;
    else if (name == QStringLiteral("Maximize"))
        return MaximizeOp;
    else if (name == QStringLiteral("Minimize"))
        return MinimizeOp;
    else if (name == QStringLiteral("Close"))
        return CloseOp;
    else if (name == QStringLiteral("OnAllDesktops"))
        return OnAllDesktopsOp;
    else if (name == QStringLiteral("Shade"))
        return ShadeOp;
    else if (name == QStringLiteral("Operations"))
        return OperationsOp;
    else if (name == QStringLiteral("Maximize (vertical only)"))
        return VMaximizeOp;
    else if (name == QStringLiteral("Maximize (horizontal only)"))
        return HMaximizeOp;
    else if (name == QStringLiteral("Lower"))
        return LowerOp;
    return NoOp;
}

Options::MouseCommand Options::mouseCommand(const QString &name, bool restricted)
{
    QString lowerName = name.toLower();
    if (lowerName == QStringLiteral("raise")) return MouseRaise;
    if (lowerName == QStringLiteral("lower")) return MouseLower;
    if (lowerName == QStringLiteral("operations menu")) return MouseOperationsMenu;
    if (lowerName == QStringLiteral("toggle raise and lower")) return MouseToggleRaiseAndLower;
    if (lowerName == QStringLiteral("activate and raise")) return MouseActivateAndRaise;
    if (lowerName == QStringLiteral("activate and lower")) return MouseActivateAndLower;
    if (lowerName == QStringLiteral("activate")) return MouseActivate;
    if (lowerName == QStringLiteral("activate, raise and pass click")) return MouseActivateRaiseAndPassClick;
    if (lowerName == QStringLiteral("activate and pass click")) return MouseActivateAndPassClick;
    if (lowerName == QStringLiteral("scroll")) return MouseNothing;
    if (lowerName == QStringLiteral("activate and scroll")) return MouseActivateAndPassClick;
    if (lowerName == QStringLiteral("activate, raise and scroll")) return MouseActivateRaiseAndPassClick;
    if (lowerName == QStringLiteral("activate, raise and move"))
        return restricted ? MouseActivateRaiseAndMove : MouseActivateRaiseAndUnrestrictedMove;
    if (lowerName == QStringLiteral("move")) return restricted ? MouseMove : MouseUnrestrictedMove;
    if (lowerName == QStringLiteral("resize")) return restricted ? MouseResize : MouseUnrestrictedResize;
    if (lowerName == QStringLiteral("shade")) return MouseShade;
    if (lowerName == QStringLiteral("minimize")) return MouseMinimize;
    if (lowerName == QStringLiteral("close")) return MouseClose;
    if (lowerName == QStringLiteral("increase opacity")) return MouseOpacityMore;
    if (lowerName == QStringLiteral("decrease opacity")) return MouseOpacityLess;
    if (lowerName == QStringLiteral("nothing")) return MouseNothing;
    return MouseNothing;
}

Options::MouseWheelCommand Options::mouseWheelCommand(const QString &name)
{
    QString lowerName = name.toLower();
    if (lowerName == QStringLiteral("raise/lower")) return MouseWheelRaiseLower;
    if (lowerName == QStringLiteral("shade/unshade")) return MouseWheelShadeUnshade;
    if (lowerName == QStringLiteral("maximize/restore")) return MouseWheelMaximizeRestore;
    if (lowerName == QStringLiteral("above/below")) return MouseWheelAboveBelow;
    if (lowerName == QStringLiteral("previous/next desktop")) return MouseWheelPreviousNextDesktop;
    if (lowerName == QStringLiteral("change opacity")) return MouseWheelChangeOpacity;
    if (lowerName == QStringLiteral("nothing")) return MouseWheelNothing;
    return MouseWheelNothing;
}

bool Options::showGeometryTip() const
{
    return show_geometry_tip;
}

bool Options::condensedTitle() const
{
    return condensed_title;
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
    default:
        return MouseNothing;
    }
}
#endif

double Options::animationTimeFactor() const
{
 #ifndef KCMRULES
    return m_settings->animationDurationFactor();
#else
    return 0;
#endif
}

Options::WindowOperation Options::operationMaxButtonClick(Qt::MouseButtons button) const
{
    return button == Qt::RightButton ? opMaxButtonRightClick :
           button == Qt::MiddleButton ?   opMaxButtonMiddleClick :
           opMaxButtonLeftClick;
}

QStringList Options::modifierOnlyDBusShortcut(Qt::KeyboardModifier mod) const
{
    return m_modifierOnlyShortcuts.value(mod);
}

bool Options::isUseCompositing() const
{
    return m_useCompositing || kwinApp()->platform()->requiresCompositing();
}

} // namespace
