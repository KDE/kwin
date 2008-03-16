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

#ifndef KCMRULES

#include <QPalette>
#include <QPixmap>
#include <kapplication.h>
#include <kconfig.h>
#include <kglobal.h>
#include <kglobalsettings.h>

#include <QDesktopWidget>

#include "client.h"
#include "compositingprefs.h"

#endif

namespace KWin
{

#ifndef KCMRULES

Options::Options()
    :   electric_borders( 0 ),
        electric_border_delay(0)
    {
    updateSettings();
    }

Options::~Options()
    {
    }

unsigned long Options::updateSettings()
    {
    KSharedConfig::Ptr _config = KGlobal::config();
    unsigned long changed = 0;
    changed |= KDecorationOptions::updateSettings( _config.data() ); // read decoration settings

    KConfigGroup config(_config, "Windows");
    moveMode = stringToMoveResizeMode( config.readEntry("MoveMode", "Opaque" ));
    resizeMode = stringToMoveResizeMode( config.readEntry("ResizeMode", "Opaque" ));
    show_geometry_tip = config.readEntry("GeometryTip", false);

    QString val;

    val = config.readEntry ("FocusPolicy", "ClickToFocus");
    focusPolicy = ClickToFocus; // what a default :-)
    if ( val == "FocusFollowsMouse" )
        focusPolicy = FocusFollowsMouse;
    else if ( val == "FocusUnderMouse" )
        focusPolicy = FocusUnderMouse;
    else if ( val == "FocusStrictlyUnderMouse" )
        focusPolicy = FocusStrictlyUnderMouse;

    val = config.readEntry ("AltTabStyle", "KDE");
    altTabStyle = KDE; // what a default :-)
    if ( val == "CDE" )
        altTabStyle = CDE;
        
    separateScreenFocus = config.readEntry( "SeparateScreenFocus", false );
    activeMouseScreen = config.readEntry( "ActiveMouseScreen", focusPolicy != ClickToFocus );

    rollOverDesktops = config.readEntry("RollOverDesktops", true);

//    focusStealingPreventionLevel = config.readEntry( "FocusStealingPreventionLevel", 2 );
    // TODO use low level for now
    focusStealingPreventionLevel = config.readEntry( "FocusStealingPreventionLevel", 1 );
    focusStealingPreventionLevel = qMax( 0, qMin( 4, focusStealingPreventionLevel ));
    if( !focusPolicyIsReasonable()) // #48786, comments #7 and later
        focusStealingPreventionLevel = 0;

    KConfigGroup gWindowsConfig( _config, "Windows"); // from kdeglobals
    xineramaEnabled = gWindowsConfig.readEntry ("XineramaEnabled", true);
    xineramaPlacementEnabled = gWindowsConfig.readEntry ("XineramaPlacementEnabled", true);
    xineramaMovementEnabled = gWindowsConfig.readEntry ("XineramaMovementEnabled", true);
    xineramaMaximizeEnabled = gWindowsConfig.readEntry ("XineramaMaximizeEnabled", true);
    xineramaFullscreenEnabled = gWindowsConfig.readEntry ("XineramaFullscreenEnabled", true);

    placement = Placement::policyFromString( config.readEntry("Placement"), true );
    xineramaPlacementScreen = qBound( -1, config.readEntry( "XineramaPlacementScreen", -1 ),
        qApp->desktop()->numScreens() - 1 );

    if( focusPolicy == ClickToFocus )
        {
        autoRaise = false;
        autoRaiseInterval = 0;
        delayFocus = false;
        delayFocusInterval = 0;
        }
    else
        {
        autoRaise = config.readEntry("AutoRaise", false);
        autoRaiseInterval = config.readEntry("AutoRaiseInterval", 0 );
        delayFocus = config.readEntry("DelayFocus", false);
        delayFocusInterval = config.readEntry("DelayFocusInterval", 0 );
        }

    shadeHover = config.readEntry("ShadeHover", false);
    shadeHoverInterval = config.readEntry("ShadeHoverInterval", 250 );

    // important: autoRaise implies ClickRaise
    clickRaise = autoRaise || config.readEntry("ClickRaise", true);

    borderSnapZone = config.readEntry("BorderSnapZone", 10);
    windowSnapZone = config.readEntry("WindowSnapZone", 10);
    snapOnlyWhenOverlapping = config.readEntry("SnapOnlyWhenOverlapping", false);
    electric_borders = config.readEntry("ElectricBorders", 0);
    electric_border_delay = config.readEntry("ElectricBorderDelay", 150);

    OpTitlebarDblClick = windowOperation( config.readEntry("TitlebarDoubleClickCommand", "Shade"), true );
    setOpMaxButtonLeftClick( windowOperation( config.readEntry("MaximizeButtonLeftClickCommand", "Maximize"), true ));
    setOpMaxButtonMiddleClick( windowOperation( config.readEntry("MaximizeButtonMiddleClickCommand", "Maximize (vertical only)"), true ));
    setOpMaxButtonRightClick( windowOperation( config.readEntry("MaximizeButtonRightClickCommand", "Maximize (horizontal only)"), true ));

    ignorePositionClasses = config.readEntry("IgnorePositionClasses",QStringList());
    ignoreFocusStealingClasses = config.readEntry("IgnoreFocusStealingClasses",QStringList());
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

    killPingTimeout = config.readEntry( "KillPingTimeout", 5000 );
    hideUtilityWindowsForInactive = config.readEntry( "HideUtilityWindowsForInactive", true);
    showDesktopIsMinimizeAll = config.readEntry( "ShowDesktopIsMinimizeAll", false );

    // Mouse bindings
    config = KConfigGroup(_config,"MouseBindings");
    CmdActiveTitlebar1 = mouseCommand(config.readEntry("CommandActiveTitlebar1","Raise"), true );
    CmdActiveTitlebar2 = mouseCommand(config.readEntry("CommandActiveTitlebar2","Lower"), true );
    CmdActiveTitlebar3 = mouseCommand(config.readEntry("CommandActiveTitlebar3","Operations menu"), true );
    CmdInactiveTitlebar1 = mouseCommand(config.readEntry("CommandInactiveTitlebar1","Activate and raise"), true );
    CmdInactiveTitlebar2 = mouseCommand(config.readEntry("CommandInactiveTitlebar2","Activate and lower"), true );
    CmdInactiveTitlebar3 = mouseCommand(config.readEntry("CommandInactiveTitlebar3","Operations menu"), true );
    CmdTitlebarWheel = mouseWheelCommand(config.readEntry("CommandTitlebarWheel","Nothing"));
    CmdWindow1 = mouseCommand(config.readEntry("CommandWindow1","Activate, raise and pass click"), false );
    CmdWindow2 = mouseCommand(config.readEntry("CommandWindow2","Activate and pass click"), false );
    CmdWindow3 = mouseCommand(config.readEntry("CommandWindow3","Activate and pass click"), false );
    CmdAllModKey = (config.readEntry("CommandAllKey","Alt") == "Meta") ? Qt::Key_Meta : Qt::Key_Alt;
    CmdAll1 = mouseCommand(config.readEntry("CommandAll1","Move"), false );
    CmdAll2 = mouseCommand(config.readEntry("CommandAll2","Toggle raise and lower"), false );
    CmdAll3 = mouseCommand(config.readEntry("CommandAll3","Resize"), false );
    CmdAllWheel = mouseWheelCommand(config.readEntry("CommandAllWheel","Nothing"));

    config=KConfigGroup(_config,"Compositing");
    refreshRate = config.readEntry( "RefreshRate", 0 );

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

    // Driver-specific config detection
    CompositingPrefs prefs;
    prefs.detect();
    reloadCompositingSettings( prefs );

    return changed;
    }

void Options::reloadCompositingSettings(const CompositingPrefs& prefs)
    {
    KSharedConfig::Ptr _config = KGlobal::config();
    KConfigGroup config(_config, "Compositing");

    // Compositing settings
    useCompositing = config.readEntry("Enabled", prefs.enableCompositing());
    QString compositingBackend = config.readEntry("Backend", "OpenGL");
    if( compositingBackend == "XRender" )
        compositingMode = XRenderCompositing;
    else
        compositingMode = OpenGLCompositing;
    QString glmode = config.readEntry("GLMode", "TFP" ).toUpper();
    if( glmode == "TFP" )
        glMode = GLTFP;
    else if( glmode == "SHM" )
        glMode = GLSHM;
    else
        glMode = GLFallback;
    glDirect = config.readEntry("GLDirect", prefs.enableDirectRendering() );
    glVSync = config.readEntry("GLVSync", prefs.enableVSync() );
    smoothScale = qBound( -1, config.readEntry( "GLTextureFilter", -1 ), 2 );
    glStrictBinding = config.readEntry( "GLStrictBinding", prefs.strictBinding());

    xrenderSmoothScale = config.readEntry("XRenderSmoothScale", false );

    const HiddenPreviews hps[] = { HiddenPreviewsNever, HiddenPreviewsKeep, HiddenPreviewUpdate, HiddenPreviewsActive };
    hiddenPreviews = hps[ qBound( 0, config.readEntry( "HiddenPreviews", 0 ), 3 ) ];
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
