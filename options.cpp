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

Options::Options()
    : QObject( 0, 0)
{
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
}

const QColor& Options::color(ColorType type, bool active)
{
    return(colors[type + (active ? 0 : KWINCOLORS)]);
}

const QFont& Options::font(bool active)
{
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
    colors[Handle] = QColor( 140, 140, 140 );
    colors[Handle] = config->readColorEntry("handle", &colors[Handle]);

    colors[Groove] = colors[Frame];
    colors[Groove] = config->readColorEntry("activeGroove",
                                            &colors[Groove]);

    if(qGray(colors[Frame].rgb()) > 150)
        colors[GrooveText] =  Qt::black;
    else
        colors[GrooveText] = Qt::white;
    colors[GrooveText] = config->readColorEntry("activeGrooveText",
                                                &colors[GrooveText]);

    // full button configuration (background, blend, and foreground
    if(QPixmap::defaultDepth() > 8)
        colors[ButtonBg] = colors[Frame].light(130);
    else
        colors[ButtonBg] = colors[Frame];
    colors[ButtonBg] = config->readColorEntry("activeTitleBtnBg",
                                              &colors[Frame]);
    if(QPixmap::defaultDepth() > 8)
        colors[ButtonBlend] = colors[Frame].dark(130);
    else
        colors[ButtonBlend] = colors[Frame];
    colors[ButtonBlend] = config->readColorEntry("activeTitleBtnBlend",
                                                 &colors[ButtonBlend]);
    if(qGray(colors[ButtonBg].rgb()) > 150 ||
       qGray(colors[ButtonBlend].rgb()) > 150)
        colors[ButtonFg] =  Qt::black;
    else
        colors[ButtonFg] = Qt::white;
    colors[ButtonFg] = config->readColorEntry("activeTitleBtnFg",
                                              &colors[ButtonFg]);

    // single color button configuration
    colors[ButtonSingleColor] = Qt::darkGray;
    colors[ButtonSingleColor] =
        config->readColorEntry("activeTitleBtnFg", &colors[ButtonSingleColor]);

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
    colors[Groove+KWINCOLORS] =
        config->readColorEntry("inactiveGroove", &colors[Frame+KWINCOLORS]);

    if(qGray(colors[Frame+KWINCOLORS].rgb()) > 150)
        colors[GrooveText+KWINCOLORS] =  Qt::darkGray;
    else
        colors[GrooveText+KWINCOLORS] = Qt::lightGray;
    colors[GrooveText+KWINCOLORS] =
        config->readColorEntry("inactiveGrooveText",
                               &colors[GrooveText+KWINCOLORS]);

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

    if(QPixmap::defaultDepth() > 8)
        colors[ButtonBlend+KWINCOLORS] = colors[ Frame+KWINCOLORS ].dark(130);
    else
        colors[ButtonBlend+KWINCOLORS] = colors[ Frame+KWINCOLORS ];
    colors[ButtonBlend+KWINCOLORS] =
        config->readColorEntry("inactiveTitleBtnBlend",
                               &colors[ButtonBlend+KWINCOLORS]);

    colors[ButtonFg+KWINCOLORS] = config->
        readColorEntry("inactiveTitleBtnFg", &colors[ButtonFg]);

    // single color
    colors[ButtonSingleColor+KWINCOLORS] = config->
        readColorEntry("inactiveTitleBtnFg", &colors[ButtonSingleColor]);


    colors[Handle+KWINCOLORS] = colors[Frame];
        config->readColorEntry("inactiveHandle", &colors[Handle]);

    colors[Font+KWINCOLORS] = colors[Frame].dark();
    colors[Font+KWINCOLORS] = config->readColorEntry("inactiveForeground",
                                                     &colors[Font+KWINCOLORS]);

    activeFont = QFont("Helvetica", 12, QFont::Bold);
    activeFont = config->readFontEntry("activeFont", &activeFont);
    inactiveFont = config->readFontEntry("inactiveFont", &activeFont);

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


    QString val;

    val = config->readEntry ("focusPolicy", "ClickToFocus");
    if (val == "ClickToFocus")
	focusPolicy = ClickToFocus;
    else
	focusPolicy = FocusFollowsMouse;

    val = config->readEntry("Placement","Smart");
    if (val == "Smart") placement = Smart;
    else if (val == "Random") placement = Random;
    else if (val == "Cascade") placement = Cascade;

    animate_shade = config->readBoolEntry("AnimateShade", TRUE );

    anim_steps = config->readNumEntry("AnimSteps", 10);

    border_snap_zone = config->readNumEntry("BorderSnapZone", 10);
    window_snap_zone = config->readNumEntry("WindowSnapZone", 10);


    OpTitlebarDblClick = windowOperation( config->readEntry("TitlebarDoubleClickCommand", "winShade") );

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
    CmdAll1 = mouseCommand(config->readEntry("CommandAll1","Move"));
    CmdAll2 = mouseCommand(config->readEntry("CommandAll2","Toggle raise and lower"));
    CmdAll3 = mouseCommand(config->readEntry("CommandAll3","Resize"));

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
    if (name == "Nothing") return MouseNothing;
    return MouseNothing;
}
