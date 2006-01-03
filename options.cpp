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

#include <qpalette.h>
#include <qpixmap.h>
#include <kapplication.h>
#include <kconfig.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <qtooltip.h>
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
    KConfig *config = KGlobal::config();
    unsigned long changed = 0;
    changed |= d->updateKWinSettings( config ); // read decoration settings

    config->setGroup( "Windows" );
    moveMode = stringToMoveResizeMode( config->readEntry("MoveMode", "Opaque" ));
    resizeMode = stringToMoveResizeMode( config->readEntry("ResizeMode", "Opaque" ));
    show_geometry_tip = config->readEntry("GeometryTip", QVariant(false)).toBool();

    QString val;

    val = config->readEntry ("FocusPolicy", "ClickToFocus");
    focusPolicy = ClickToFocus; // what a default :-)
    if ( val == "FocusFollowsMouse" )
        focusPolicy = FocusFollowsMouse;
    else if ( val == "FocusUnderMouse" )
        focusPolicy = FocusUnderMouse;
    else if ( val == "FocusStrictlyUnderMouse" )
        focusPolicy = FocusStrictlyUnderMouse;

    val = config->readEntry ("AltTabStyle", "KDE");
    altTabStyle = KDE; // what a default :-)
    if ( val == "CDE" )
        altTabStyle = CDE;

    rollOverDesktops = config->readEntry("RollOverDesktops", QVariant(TRUE)).toBool();
    
//    focusStealingPreventionLevel = config->readNumEntry( "FocusStealingPreventionLevel", 2 );
    // TODO use low level for now
    focusStealingPreventionLevel = config->readNumEntry( "FocusStealingPreventionLevel", 1 );
    focusStealingPreventionLevel = qMax( 0, qMin( 4, focusStealingPreventionLevel ));
    if( !focusPolicyIsReasonable()) // #48786, comments #7 and later
        focusStealingPreventionLevel = 0;

    KConfig *gc = new KConfig("kdeglobals", false, false);
    bool isVirtual = KApplication::desktop()->isVirtualDesktop();
    gc->setGroup("Windows");
    xineramaEnabled = gc->readEntry ("XineramaEnabled", QVariant(isVirtual )).toBool() &&
                      isVirtual;
    if (xineramaEnabled) 
        {
        xineramaPlacementEnabled = gc->readEntry ("XineramaPlacementEnabled", QVariant(true)).toBool();
        xineramaMovementEnabled = gc->readEntry ("XineramaMovementEnabled", QVariant(true)).toBool();
        xineramaMaximizeEnabled = gc->readEntry ("XineramaMaximizeEnabled", QVariant(true)).toBool();
        xineramaFullscreenEnabled = gc->readEntry ("XineramaFullscreenEnabled", QVariant(true)).toBool();
        }
    else 
        {
        xineramaPlacementEnabled = xineramaMovementEnabled = xineramaMaximizeEnabled = xineramaFullscreenEnabled = false;
        }
    delete gc;

    placement = Placement::policyFromString( config->readEntry("Placement"), true );

    animateShade = config->readEntry("AnimateShade", QVariant(TRUE )).toBool();

    animateMinimize = config->readEntry("AnimateMinimize", QVariant(TRUE )).toBool();
    animateMinimizeSpeed = config->readNumEntry("AnimateMinimizeSpeed", 5 );

    if( focusPolicy == ClickToFocus ) 
        {
        autoRaise = false;
        autoRaiseInterval = 0;
        delayFocus = false;
        delayFocusInterval = 0;
        }
    else 
        {
        autoRaise = config->readEntry("AutoRaise", QVariant(FALSE )).toBool();
        autoRaiseInterval = config->readNumEntry("AutoRaiseInterval", 0 );
        delayFocus = config->readEntry("DelayFocus", QVariant(FALSE )).toBool();
        delayFocusInterval = config->readNumEntry("DelayFocusInterval", 0 );
        }

    shadeHover = config->readEntry("ShadeHover", QVariant(FALSE )).toBool();
    shadeHoverInterval = config->readNumEntry("ShadeHoverInterval", 250 );

    // important: autoRaise implies ClickRaise
    clickRaise = autoRaise || config->readEntry("ClickRaise", QVariant(TRUE )).toBool();

    borderSnapZone = config->readNumEntry("BorderSnapZone", 10);
    windowSnapZone = config->readNumEntry("WindowSnapZone", 10);
    snapOnlyWhenOverlapping=config->readEntry("SnapOnlyWhenOverlapping", QVariant(FALSE)).toBool();
    electric_borders = config->readNumEntry("ElectricBorders", 0);
    electric_border_delay = config->readNumEntry("ElectricBorderDelay", 150);

    OpTitlebarDblClick = windowOperation( config->readEntry("TitlebarDoubleClickCommand", "Shade"), true );
    d->OpMaxButtonLeftClick = windowOperation( config->readEntry("MaximizeButtonLeftClickCommand", "Maximize"), true );
    d->OpMaxButtonMiddleClick = windowOperation( config->readEntry("MaximizeButtonMiddleClickCommand", "Maximize (vertical only)"), true );
    d->OpMaxButtonRightClick = windowOperation( config->readEntry("MaximizeButtonRightClickCommand", "Maximize (horizontal only)"), true );

    ignorePositionClasses = config->readListEntry("IgnorePositionClasses");
    ignoreFocusStealingClasses = config->readListEntry("IgnoreFocusStealingClasses");
    // Qt3.2 and older had resource class all lowercase, but Qt3.3 has it capitalized
    // therefore Client::resourceClass() forces lowercase, force here lowercase as well
    for( QStringList::Iterator it = ignorePositionClasses.begin();
         it != ignorePositionClasses.end();
         ++it )
        (*it) = (*it).lower();
    for( QStringList::Iterator it = ignoreFocusStealingClasses.begin();
         it != ignoreFocusStealingClasses.end();
         ++it )
        (*it) = (*it).lower();

    killPingTimeout = config->readNumEntry( "KillPingTimeout", 5000 );
    hideUtilityWindowsForInactive = config->readEntry( "HideUtilityWindowsForInactive", QVariant(false )).toBool();

    // Mouse bindings
    config->setGroup( "MouseBindings");
    CmdActiveTitlebar1 = mouseCommand(config->readEntry("CommandActiveTitlebar1","Raise"), true );
    CmdActiveTitlebar2 = mouseCommand(config->readEntry("CommandActiveTitlebar2","Lower"), true );
    CmdActiveTitlebar3 = mouseCommand(config->readEntry("CommandActiveTitlebar3","Operations menu"), true );
    CmdInactiveTitlebar1 = mouseCommand(config->readEntry("CommandInactiveTitlebar1","Activate and raise"), true );
    CmdInactiveTitlebar2 = mouseCommand(config->readEntry("CommandInactiveTitlebar2","Activate and lower"), true );
    CmdInactiveTitlebar3 = mouseCommand(config->readEntry("CommandInactiveTitlebar3","Operations menu"), true );
    CmdTitlebarWheel = mouseWheelCommand(config->readEntry("CommandTitlebarWheel","Nothing"));
    CmdWindow1 = mouseCommand(config->readEntry("CommandWindow1","Activate, raise and pass click"), false );
    CmdWindow2 = mouseCommand(config->readEntry("CommandWindow2","Activate and pass click"), false );
    CmdWindow3 = mouseCommand(config->readEntry("CommandWindow3","Activate and pass click"), false );
    CmdAllModKey = (config->readEntry("CommandAllKey","Alt") == "Meta") ? Qt::Key_Meta : Qt::Key_Alt;
    CmdAll1 = mouseCommand(config->readEntry("CommandAll1","Move"), false );
    CmdAll2 = mouseCommand(config->readEntry("CommandAll2","Toggle raise and lower"), false );
    CmdAll3 = mouseCommand(config->readEntry("CommandAll3","Resize"), false );
    CmdAllWheel = mouseWheelCommand(config->readEntry("CommandAllWheel","Nothing"));

    //translucency settings
    config->setGroup( "Notification Messages" );
    useTranslucency = config->readEntry("UseTranslucency", QVariant(false)).toBool();
    config->setGroup( "Translucency");
    translucentActiveWindows = config->readEntry("TranslucentActiveWindows", QVariant(false)).toBool();
    activeWindowOpacity = uint((config->readNumEntry("ActiveWindowOpacity", 100)/100.0)*0xFFFFFFFF);
    translucentInactiveWindows = config->readEntry("TranslucentInactiveWindows", QVariant(false)).toBool();
    inactiveWindowOpacity = uint((config->readNumEntry("InactiveWindowOpacity", 75)/100.0)*0xFFFFFFFF);
    translucentMovingWindows = config->readEntry("TranslucentMovingWindows", QVariant(false)).toBool();
    movingWindowOpacity = uint((config->readNumEntry("MovingWindowOpacity", 50)/100.0)*0xFFFFFFFF);
    translucentDocks = config->readEntry("TranslucentDocks", QVariant(false)).toBool();
    dockOpacity = uint((config->readNumEntry("DockOpacity", 80)/100.0)*0xFFFFFFFF);
    keepAboveAsActive = config->readEntry("TreatKeepAboveAsActive", QVariant(true)).toBool();
    //TODO: remove this variable
    useTitleMenuSlider = true;
    activeWindowShadowSize = config->readNumEntry("ActiveWindowShadowSize", 200);
    inactiveWindowShadowSize = config->readNumEntry("InactiveWindowShadowSize", 100);
    dockShadowSize = config->readNumEntry("DockShadowSize", 80);
    removeShadowsOnMove = config->readEntry("RemoveShadowsOnMove", QVariant(true)).toBool();
    removeShadowsOnResize = config->readEntry("RemoveShadowsOnResize", QVariant(true)).toBool();
    onlyDecoTranslucent = config->readEntry("OnlyDecoTranslucent", QVariant(false)).toBool();
    if (resetKompmgr = config->readEntry("ResetKompmgr", QVariant(false)).toBool())
        config->writeEntry("ResetKompmgr",FALSE);
    
    
    
    // Read button tooltip animation effect from kdeglobals
    // Since we want to allow users to enable window decoration tooltips
    // and not kstyle tooltips and vise-versa, we don't read the
    // "EffectNoTooltip" setting from kdeglobals.
    KConfig globalConfig("kdeglobals");
    globalConfig.setGroup("KDE");
    topmenus = globalConfig.readEntry( "macStyle", QVariant(false )).toBool();

    KConfig kdesktopcfg( "kdesktoprc", true );
    kdesktopcfg.setGroup( "Menubar" );
    desktop_topmenu = kdesktopcfg.readEntry( "ShowMenubar", QVariant(false )).toBool();
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
    QString lowerName = name.lower();
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
    QString lowerName = name.lower();
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
