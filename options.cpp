/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#include "options.h"
#include <qpalette.h>
#include <qpixmap.h>
#include <kapp.h>
#include <kconfig.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <kaccel.h> // for KAccel::keyboardHasMetaKey()

using namespace KWinInternal;

namespace KWinInternal
{
class OptionsPrivate
{
public:
    OptionsPrivate() : title_buttons_left( "MS" ), title_buttons_right( "HIAX" ),
        custom_button_positions( false ) {};
    QColor colors[KWINCOLORS*2];
    QColorGroup *cg[KWINCOLORS*2];
    QString title_buttons_left;
    QString title_buttons_right;
    bool custom_button_positions;
    bool show_tooltips;
    bool fade_tooltips;
    bool animate_tooltips;
};
};

#define colors (d->colors)
#define cg (d->cg)

Options::Options()
    : QObject( 0, 0)
{
    d = new OptionsPrivate;
    int i;
    for(i=0; i < KWINCOLORS*2; ++i)
        cg[i] = NULL;
    reload();

    connect( kapp, SIGNAL( appearanceChanged() ), this, SLOT(reload() ) );
}

Options::~Options(){
    int i;
    for(i=0; i < KWINCOLORS*2; ++i){
        if(cg[i]){
            delete cg[i];
            cg[i] = NULL;
        }
    }
    delete d;
}

const QColor& Options::color(ColorType type, bool active)
{
    return(colors[type + (active ? 0 : KWINCOLORS)]);
}

const QFont& Options::font(bool active, bool small)
{
    if ( small )
        return(active ? activeFontSmall : inactiveFontSmall);
    else
        return(active ? activeFont : inactiveFont);
}

const QColorGroup& Options::colorGroup(ColorType type, bool active)
{
    int idx = type + (active ? 0 : KWINCOLORS);
    if(cg[idx])
        return(*cg[idx]);
    cg[idx] = new QColorGroup(Qt::black, colors[idx], colors[idx].light(150),
                              colors[idx].dark(), colors[idx].dark(120),
                              Qt::black, QApplication::palette().normal().
                              base());
    return(*cg[idx]);
}

void Options::reload()
{
    QPalette pal = QApplication::palette();
    KConfig *config = KGlobal::config();
    config->setGroup("WM");

    // normal colors
    colors[Frame] = pal.normal().background();
    colors[Frame] = config->readColorEntry("frame", &colors[Frame]);
    colors[Handle] = colors[Frame];
    colors[Handle] = config->readColorEntry("handle", &colors[Handle]);

    // full button configuration (background, blend, and foreground
    if(QPixmap::defaultDepth() > 8)
        colors[ButtonBg] = colors[Frame].light(130);
    else
        colors[ButtonBg] = colors[Frame];
    colors[ButtonBg] = config->readColorEntry("activeTitleBtnBg",
                                              &colors[Frame]);
    colors[TitleBar] = pal.normal().highlight();
    colors[TitleBar] = config->readColorEntry("activeBackground",
                                              &colors[TitleBar]);
    if(QPixmap::defaultDepth() > 8)
        colors[TitleBlend] = colors[ TitleBar ].dark(110);
    else
        colors[TitleBlend] = colors[ TitleBar ];
    colors[TitleBlend] = config->readColorEntry("activeBlend",
                                                &colors[TitleBlend]);

    colors[Font] = pal.normal().highlightedText();
    colors[Font] = config->readColorEntry("activeForeground", &colors[Font]);

    // inactive
    colors[Frame+KWINCOLORS] = config->readColorEntry("inactiveFrame",
                                                      &colors[Frame]);
    colors[TitleBar+KWINCOLORS] = colors[Frame];
    colors[TitleBar+KWINCOLORS] = config->
        readColorEntry("inactiveBackground", &colors[TitleBar+KWINCOLORS]);

    if(QPixmap::defaultDepth() > 8)
        colors[TitleBlend+KWINCOLORS] = colors[ TitleBar+KWINCOLORS ].dark(110);
    else
        colors[TitleBlend+KWINCOLORS] = colors[ TitleBar+KWINCOLORS ];
    colors[TitleBlend+KWINCOLORS] =
        config->readColorEntry("inactiveBlend", &colors[TitleBlend+KWINCOLORS]);

    // full button configuration
    if(QPixmap::defaultDepth() > 8)
        colors[ButtonBg+KWINCOLORS] = colors[Frame+KWINCOLORS].light(130);
    else
        colors[ButtonBg+KWINCOLORS] = colors[Frame+KWINCOLORS];
    colors[ButtonBg+KWINCOLORS] =
        config->readColorEntry("inactiveTitleBtnBg",
                               &colors[ButtonBg]);

    colors[Handle+KWINCOLORS] = colors[Frame];
        config->readColorEntry("inactiveHandle", &colors[Handle]);

    colors[Font+KWINCOLORS] = colors[Frame].dark();
    colors[Font+KWINCOLORS] = config->readColorEntry("inactiveForeground",
                                                     &colors[Font+KWINCOLORS]);

    // Keep in sync with kglobalsettings.

    QFont activeFontGuess("helvetica", 12, QFont::Bold);
    activeFontGuess.setPixelSize(12);

    activeFont = config->readFontEntry("activeFont", &activeFontGuess);
    inactiveFont = config->readFontEntry("inactiveFont", &activeFont);

    activeFontSmall = activeFont;
    activeFontSmall.setPointSize(activeFont.pointSize() - 2);
    activeFontSmall = config->readFontEntry("activeFontSmall", &activeFontSmall);
    inactiveFontSmall = config->readFontEntry("inactiveFontSmall", &activeFontSmall);

    int i;
    for(i=0; i < KWINCOLORS*2; ++i){
        if(cg[i]){
            delete cg[i];
            cg[i] = NULL;
        }
    }

    config->setGroup( "Windows" );
    moveMode = config->readEntry("MoveMode", "Opaque" ) == "Opaque"?Opaque:Transparent;
    resizeMode = config->readEntry("ResizeMode", "Opaque" ) == "Opaque"?Opaque:Transparent;
    moveResizeMaximizedWindows = config->readBoolEntry("MoveResizeMaximizedWindows", true );


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

    xineramaEnabled = config->readBoolEntry ("XineramaEnabled", FALSE ) &&
		      KApplication::desktop()->isVirtualDesktop();
    if (xineramaEnabled) {
	xineramaPlacementEnabled = config->readBoolEntry ("XineramaPlacementEnabled", FALSE);
 	xineramaMovementEnabled = config->readBoolEntry ("XineramaMovementEnabled", FALSE);
 	xineramaMaximizeEnabled = config->readBoolEntry ("XineramaMaximizeEnabled", FALSE);
    }

    val = config->readEntry("Placement","Smart");
    if (val == "Smart") placement = Smart;
    else if (val == "Random") placement = Random;
    else if (val == "Cascade") placement = Cascade;

    animateShade = config->readBoolEntry("AnimateShade", TRUE );

    animateMinimize = config->readBoolEntry("AnimateMinimize", TRUE );
    animateMinimizeSpeed = config->readNumEntry("AnimateMinimizeSpeed", 5 );

    autoRaise = config->readBoolEntry("AutoRaise", FALSE );
    autoRaiseInterval = config->readNumEntry("AutoRaiseInterval", 0 );

    shadeHover = config->readBoolEntry("ShadeHover", FALSE );
    shadeHoverInterval = config->readNumEntry("ShadeHoverInterval", 250 );

    // important: autoRaise implies ClickRaise
    clickRaise = autoRaise || config->readBoolEntry("ClickRaise", FALSE );

    borderSnapZone = config->readNumEntry("BorderSnapZone", 10);
    windowSnapZone = config->readNumEntry("WindowSnapZone", 10);
    snapOnlyWhenOverlapping=config->readBoolEntry("SnapOnlyWhenOverlapping",FALSE);


    OpTitlebarDblClick = windowOperation( config->readEntry("TitlebarDoubleClickCommand", "Shade") );

    ignorePositionClasses = config->readListEntry("IgnorePositionClasses");


    // desktop settings

    config->setGroup("Desktops");
    desktopRows = config->readNumEntry( "DesktopRows", 2 );
    if ( desktopRows < 1 )
        desktopRows = 1;

    // Mouse bindings
    config->setGroup( "MouseBindings");
    CmdActiveTitlebar1 = mouseCommand(config->readEntry("CommandActiveTitlebar1","Raise"));
    CmdActiveTitlebar2 = mouseCommand(config->readEntry("CommandActiveTitlebar2","Lower"));
    CmdActiveTitlebar3 = mouseCommand(config->readEntry("CommandActiveTitlebar3","Operations menu"));
    CmdInactiveTitlebar1 = mouseCommand(config->readEntry("CommandInactiveTitlebar1","Activate and raise"));
    CmdInactiveTitlebar2 = mouseCommand(config->readEntry("CommandInactiveTitlebar2","Activate and lower"));
    CmdInactiveTitlebar3 = mouseCommand(config->readEntry("CommandInactiveTitlebar3","Activate"));
    CmdWindow1 = mouseCommand(config->readEntry("CommandWindow1","Activate, raise and pass click"));
    CmdWindow2 = mouseCommand(config->readEntry("CommandWindow2","Activate and pass click"));
    CmdWindow3 = mouseCommand(config->readEntry("CommandWindow3","Activate and pass click"));
    CmdAllModKey = (config->readEntry("CommandAllKey", KAccel::keyboardHasMetaKey() ? "Meta" : "Alt") == "Meta") ? Qt::Key_Meta : Qt::Key_Alt;
    CmdAll1 = mouseCommand(config->readEntry("CommandAll1","Move"));
    CmdAll2 = mouseCommand(config->readEntry("CommandAll2","Toggle raise and lower"));
    CmdAll3 = mouseCommand(config->readEntry("CommandAll3","Resize"));

    // custom button positions
    config->setGroup("Style");
    d->custom_button_positions = config->readBoolEntry("CustomButtonPositions", false);
    if (d->custom_button_positions) {
	d->title_buttons_left  = config->readEntry("ButtonsOnLeft", "MS");
	d->title_buttons_right = config->readEntry("ButtonsOnRight", "HIAX");
    }
    else {
	d->title_buttons_left  = "MS";
	d->title_buttons_right = "HIAX";
    }

    // button tooltips
    d->show_tooltips = config->readBoolEntry("ShowToolTips", true);

    // Read button tooltip animation effect from kdeglobals
    // Since we want to allow users to enable window decoration tooltips
    // and not kstyle tooltips and vise-versa, we don't read the
    // "EffectNoTooltip" setting from kdeglobals.
    KConfig globalConfig("kdeglobals");
    globalConfig.setGroup("KDE");
    d->fade_tooltips = globalConfig.readBoolEntry("EffectFadeTooltip", false);
    d->animate_tooltips = globalConfig.readBoolEntry("EffectAnimateTooltip", false);

    emit resetPlugin();
    emit resetClients();
}


Options::WindowOperation Options::windowOperation(const QString &name){
    if (name == "Move")
        return MoveOp;
    else if (name == "Resize")
        return ResizeOp;
    else if (name == "Maximize")
        return MaximizeOp;
    else if (name == "Iconify")
        return IconifyOp;
    else if (name == "Close")
        return CloseOp;
    else if (name == "Sticky")
        return StickyOp;
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

Options::MouseCommand Options::mouseCommand(const QString &name)
{
    if (name == "Raise") return MouseRaise;
    if (name == "Lower") return MouseLower;
    if (name == "Operations menu") return MouseOperationsMenu;
    if (name == "Toggle raise and lower") return MouseToggleRaiseAndLower;
    if (name == "Activate and raise") return MouseActivateAndRaise;
    if (name == "Activate and lower") return MouseActivateAndLower;
    if (name == "Activate") return MouseActivate;
    if (name == "Activate, raise and pass click") return MouseActivateRaiseAndPassClick;
    if (name == "Activate and pass click") return MouseActivateAndPassClick;
    if (name == "Move") return MouseMove;
    if (name == "Resize") return MouseResize;
    if (name == "Shade") return MouseShade;
    if (name == "Nothing") return MouseNothing;
    return MouseNothing;
}

QString Options::titleButtonsLeft()
{
    return d->title_buttons_left;
}

QString Options::titleButtonsRight()
{
    return d->title_buttons_right;
}

bool Options::customButtonPositions()
{
    return d->custom_button_positions;
}

bool Options::showTooltips()
{
    return d->show_tooltips;
}

bool Options::fadeTooltips()
{
    return d->fade_tooltips;
}

bool Options::animateTooltips()
{
    return d->animate_tooltips;
}

#include "options.moc"

