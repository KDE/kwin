/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "options.h"

#ifndef KCMRULES

#include <QPalette>
#include <QPixmap>
#include <kapplication.h>
#include <kconfig.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <QToolTip>
#include <QDesktopWidget>

#include "client.h"

#endif

namespace KWinInternal
{

#ifndef KCMRULES

Options::Options()
    :   electric_borders( 0 ),
        electric_border_delay(0)
    {
    d = new KDecorationOptionsPrivate;
    d->defaultKWinSettings();
    updateSettings();
    }

Options::~Options()
    {
    delete d;
    }

unsigned long Options::updateSettings()
    {
    KSharedConfig::Ptr config = KGlobal::config();
    unsigned long changed = 0;
    changed |= d->updateKWinSettings( config.data() ); // read decoration settings

    KConfigGroup windowsConfig(config, "Windows");
    moveMode = stringToMoveResizeMode( windowsConfig.readEntry("MoveMode", "Opaque" ));
    resizeMode = stringToMoveResizeMode( windowsConfig.readEntry("ResizeMode", "Opaque" ));
    show_geometry_tip = windowsConfig.readEntry("GeometryTip", false);

    QString val;

    val = windowsConfig.readEntry ("FocusPolicy", "ClickToFocus");
    focusPolicy = ClickToFocus; // what a default :-)
    if ( val == "FocusFollowsMouse" )
        focusPolicy = FocusFollowsMouse;
    else if ( val == "FocusUnderMouse" )
        focusPolicy = FocusUnderMouse;
    else if ( val == "FocusStrictlyUnderMouse" )
        focusPolicy = FocusStrictlyUnderMouse;

    val = windowsConfig.readEntry ("AltTabStyle", "KDE");
    altTabStyle = KDE; // what a default :-)
    if ( val == "CDE" )
        altTabStyle = CDE;

    rollOverDesktops = windowsConfig.readEntry("RollOverDesktops", true);

//    focusStealingPreventionLevel = config->readEntry( "FocusStealingPreventionLevel", 2 );
    // TODO use low level for now
    focusStealingPreventionLevel = windowsConfig.readEntry( "FocusStealingPreventionLevel", 1 );
    focusStealingPreventionLevel = qMax( 0, qMin( 4, focusStealingPreventionLevel ));
    if( !focusPolicyIsReasonable()) // #48786, comments #7 and later
        focusStealingPreventionLevel = 0;

    KConfig *gc = new KConfig("kdeglobals", KConfig::NoGlobals);
    bool isVirtual = KApplication::desktop()->isVirtualDesktop();
    KConfigGroup gWindowsConfig(gc, "Windows");
    xineramaEnabled = gWindowsConfig.readEntry ("XineramaEnabled", isVirtual) &&
                      isVirtual;
    if (xineramaEnabled)
        {
        xineramaPlacementEnabled = gWindowsConfig.readEntry ("XineramaPlacementEnabled", true);
        xineramaMovementEnabled = gWindowsConfig.readEntry ("XineramaMovementEnabled", true);
        xineramaMaximizeEnabled = gWindowsConfig.readEntry ("XineramaMaximizeEnabled", true);
        xineramaFullscreenEnabled = gWindowsConfig.readEntry ("XineramaFullscreenEnabled", true);
        }
    else
        {
        xineramaPlacementEnabled = xineramaMovementEnabled = xineramaMaximizeEnabled = xineramaFullscreenEnabled = false;
        }
    delete gc;

    placement = Placement::policyFromString( windowsConfig.readEntry("Placement"), true );

    animateShade = windowsConfig.readEntry("AnimateShade", true);

    animateMinimize = windowsConfig.readEntry("AnimateMinimize", true);
    animateMinimizeSpeed = windowsConfig.readEntry("AnimateMinimizeSpeed", 5 );

    if( focusPolicy == ClickToFocus )
        {
        autoRaise = false;
        autoRaiseInterval = 0;
        delayFocus = false;
        delayFocusInterval = 0;
        }
    else
        {
        autoRaise = windowsConfig.readEntry("AutoRaise", false);
        autoRaiseInterval = windowsConfig.readEntry("AutoRaiseInterval", 0 );
        delayFocus = windowsConfig.readEntry("DelayFocus", false);
        delayFocusInterval = windowsConfig.readEntry("DelayFocusInterval", 0 );
        }

    shadeHover = windowsConfig.readEntry("ShadeHover", false);
    shadeHoverInterval = windowsConfig.readEntry("ShadeHoverInterval", 250 );

    // important: autoRaise implies ClickRaise
    clickRaise = autoRaise || windowsConfig.readEntry("ClickRaise", true);

    borderSnapZone = windowsConfig.readEntry("BorderSnapZone", 10);
    windowSnapZone = windowsConfig.readEntry("WindowSnapZone", 10);
    snapOnlyWhenOverlapping = windowsConfig.readEntry("SnapOnlyWhenOverlapping", false);
    electric_borders = windowsConfig.readEntry("ElectricBorders", 0);
    electric_border_delay = windowsConfig.readEntry("ElectricBorderDelay", 150);

    OpTitlebarDblClick = windowOperation( windowsConfig.readEntry("TitlebarDoubleClickCommand", "Shade"), true );
    d->OpMaxButtonLeftClick = windowOperation( windowsConfig.readEntry("MaximizeButtonLeftClickCommand", "Maximize"), true );
    d->OpMaxButtonMiddleClick = windowOperation( windowsConfig.readEntry("MaximizeButtonMiddleClickCommand", "Maximize (vertical only)"), true );
    d->OpMaxButtonRightClick = windowOperation( windowsConfig.readEntry("MaximizeButtonRightClickCommand", "Maximize (horizontal only)"), true );

    ignorePositionClasses = windowsConfig.readEntry("IgnorePositionClasses",QStringList());
    ignoreFocusStealingClasses = windowsConfig.readEntry("IgnoreFocusStealingClasses",QStringList());
    // Qt3.2 and older had resource class all lowercase, but Qt3.3 has it capitalized
    // therefore Client::resourceClass() forces lowercase, force here lowercase as well
    for( QStringList::Iterator it = ignorePositionClasses.begin();
         it != ignorePositionClasses.end();
         ++it )
        (*it) = (*it).toLower();
    for( QStringList::Iterator it = ignoreFocusStealingClasses.begin();
         it != ignoreFocusStealingClasses.end();
         ++it )
        (*it) = (*it).toLower();

    killPingTimeout = windowsConfig.readEntry( "KillPingTimeout", 5000 );
    hideUtilityWindowsForInactive = windowsConfig.readEntry( "HideUtilityWindowsForInactive", true);
    showDesktopIsMinimizeAll = windowsConfig.readEntry( "ShowDesktopIsMinimizeAll", false );

    // Mouse bindings
    KConfigGroup mouseConfig(config, "MouseBindings");
    CmdActiveTitlebar1 = mouseCommand(mouseConfig.readEntry("CommandActiveTitlebar1","Raise"), true );
    CmdActiveTitlebar2 = mouseCommand(mouseConfig.readEntry("CommandActiveTitlebar2","Lower"), true );
    CmdActiveTitlebar3 = mouseCommand(mouseConfig.readEntry("CommandActiveTitlebar3","Operations menu"), true );
    CmdInactiveTitlebar1 = mouseCommand(mouseConfig.readEntry("CommandInactiveTitlebar1","Activate and raise"), true );
    CmdInactiveTitlebar2 = mouseCommand(mouseConfig.readEntry("CommandInactiveTitlebar2","Activate and lower"), true );
    CmdInactiveTitlebar3 = mouseCommand(mouseConfig.readEntry("CommandInactiveTitlebar3","Operations menu"), true );
    CmdTitlebarWheel = mouseWheelCommand(mouseConfig.readEntry("CommandTitlebarWheel","Nothing"));
    CmdWindow1 = mouseCommand(mouseConfig.readEntry("CommandWindow1","Activate, raise and pass click"), false );
    CmdWindow2 = mouseCommand(mouseConfig.readEntry("CommandWindow2","Activate and pass click"), false );
    CmdWindow3 = mouseCommand(mouseConfig.readEntry("CommandWindow3","Activate and pass click"), false );
    CmdAllModKey = (mouseConfig.readEntry("CommandAllKey","Alt") == "Meta") ? Qt::Key_Meta : Qt::Key_Alt;
    CmdAll1 = mouseCommand(mouseConfig.readEntry("CommandAll1","Move"), false );
    CmdAll2 = mouseCommand(mouseConfig.readEntry("CommandAll2","Toggle raise and lower"), false );
    CmdAll3 = mouseCommand(mouseConfig.readEntry("CommandAll3","Resize"), false );
    CmdAllWheel = mouseWheelCommand(mouseConfig.readEntry("CommandAllWheel","Nothing"));

    //translucency settings - TODO
    useTranslucency = config->group("Notification Messages").readEntry("UseTranslucency", false);
    KConfigGroup translucencyConfig(config, "Translucency");
    translucentActiveWindows = translucencyConfig.readEntry("TranslucentActiveWindows", false);
    activeWindowOpacity = uint((translucencyConfig.readEntry("ActiveWindowOpacity", 100)/100.0)*0xFFFFFFFF);
    translucentInactiveWindows = translucencyConfig.readEntry("TranslucentInactiveWindows", false);
    inactiveWindowOpacity = uint((translucencyConfig.readEntry("InactiveWindowOpacity", 75)/100.0)*0xFFFFFFFF);
    translucentMovingWindows = translucencyConfig.readEntry("TranslucentMovingWindows", false);
    movingWindowOpacity = uint((translucencyConfig.readEntry("MovingWindowOpacity", 50)/100.0)*0xFFFFFFFF);
    translucentDocks = translucencyConfig.readEntry("TranslucentDocks", false);
    dockOpacity = uint((translucencyConfig.readEntry("DockOpacity", 80)/100.0)*0xFFFFFFFF);
    keepAboveAsActive = translucencyConfig.readEntry("TreatKeepAboveAsActive", true);
    //TODO: remove this variable
    useTitleMenuSlider = true;
    activeWindowShadowSize = translucencyConfig.readEntry("ActiveWindowShadowSize", 200);
    inactiveWindowShadowSize = translucencyConfig.readEntry("InactiveWindowShadowSize", 100);
    dockShadowSize = translucencyConfig.readEntry("DockShadowSize", 80);
    removeShadowsOnMove = translucencyConfig.readEntry("RemoveShadowsOnMove", true);
    removeShadowsOnResize = translucencyConfig.readEntry("RemoveShadowsOnResize", true);
    onlyDecoTranslucent = translucencyConfig.readEntry("OnlyDecoTranslucent", false);

    // Read button tooltip animation effect from kdeglobals
    // Since we want to allow users to enable window decoration tooltips
    // and not kstyle tooltips and vise-versa, we don't read the
    // "EffectNoTooltip" setting from kdeglobals.
    KConfig _globalConfig( "kdeglobals" );
    KConfigGroup globalConfig(&_globalConfig, "KDE");
    topmenus = globalConfig.readEntry( "macStyle", false);

    KConfig _kdesktopcfg( "kdesktoprc" );
    KConfigGroup kdesktopcfg(&_kdesktopcfg, "Menubar" );
    desktop_topmenu = kdesktopcfg.readEntry( "ShowMenubar", false);
    if( desktop_topmenu )
        topmenus = true;

//    QToolTip::setGloballyEnabled( d->show_tooltips );
// KDE4 this probably needs to be done manually in clients

    return changed;
    }


// restricted should be true for operations that the user may not be able to repeat
// if the window is moved out of the workspace (e.g. if the user moves a window
// by the titlebar, and moves it too high beneath Kicker at the top edge, they
// may not be able to move it back, unless they know about Alt+LMB)
Options::WindowOperation Options::windowOperation(const QString &name, bool restricted )
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

Options::MouseCommand Options::mouseCommand(const QString &name, bool restricted )
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
    if (lowerName == "activate, raise and move")
        return restricted ? MouseActivateRaiseAndMove : MouseActivateRaiseAndUnrestrictedMove;
    if (lowerName == "move") return restricted ? MouseMove : MouseUnrestrictedMove;
    if (lowerName == "resize") return restricted ? MouseResize : MouseUnrestrictedResize;
    if (lowerName == "shade") return MouseShade;
    if (lowerName == "minimize") return MouseMinimize;
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
    return MouseWheelNothing;
    }

bool Options::showGeometryTip()
    {
    return show_geometry_tip;
    }

int Options::electricBorders()
    {
    return electric_borders;
    }

int Options::electricBorderDelay()
    {
    return electric_border_delay;
    }

bool Options::checkIgnoreFocusStealing( const Client* c )
    {
    return ignoreFocusStealingClasses.contains(QString::fromLatin1(c->resourceClass()));
    }

Options::MouseCommand Options::wheelToMouseCommand( MouseWheelCommand com, int delta )
    {
    switch( com )
        {
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

Options::MoveResizeMode Options::stringToMoveResizeMode( const QString& s )
    {
    return s == "Opaque" ? Opaque : Transparent;
    }

const char* Options::moveResizeModeToString( MoveResizeMode mode )
    {
    return mode == Opaque ? "Opaque" : "Transparent";
    }

} // namespace
