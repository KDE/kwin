#include "options.h"
#include <qpalette.h>
#include <qpixmap.h>
#include <qapplication.h>
#include <kconfig.h>
#include <kglobal.h>

Options::Options()
{
    int i;
    for(i=0; i < KWINCOLORS*2; ++i)
        cg[i] = NULL;
    reload();
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
    focusPolicy = ClickToFocus;
    resizeMode = Opaque;
    moveMode = Opaque;// Transparent;

    QPalette pal = QApplication::palette();
    KConfig *config = KGlobal::config();
    config->setGroup("WM");

    // normal colors
    colors[Frame] = Qt::lightGray;
    colors[Frame] = config->readColorEntry("frame", &colors[Frame]);
    colors[Handle] = QColor( 140, 140, 140 );
    colors[Handle] = config->readColorEntry("handle", &colors[Handle]);
    colors[ButtonBg] = colors[Frame];
    colors[ButtonBg] = config->readColorEntry("buttonBackgroundDown",
                                              &colors[Frame]);
    if(QPixmap::defaultDepth() < 15)
        colors[ButtonBlend] = colors[ ButtonBg ];
    else
        colors[ButtonBlend] = colors[ ButtonBg ].dark(150);
    colors[ButtonBlend] = config->readColorEntry("buttonBlendDown",
                                                 &colors[ButtonBlend]);
    colors[TitleBar] = Qt::darkBlue;
    colors[TitleBar] = config->readColorEntry("activeBackground",
                                              &colors[TitleBar]);
    if(QPixmap::defaultDepth() < 15)
        colors[TitleBlend] = colors[ TitleBar ];
    else
        colors[TitleBlend] = colors[ TitleBar ].dark(150);
    colors[TitleBlend] = config->readColorEntry("activeBlend",
                                                &colors[TitleBlend]);

    colors[Font] = Qt::white;
    colors[Font] = config->readColorEntry("activeForeground", &colors[Font]);
    colors[ButtonFg] = Qt::darkGray;
    colors[ButtonFg] = config->readColorEntry("buttonForegroundDown",
                                              &colors[ButtonFg]);

    // inactive
    colors[Frame+KWINCOLORS] = config->readColorEntry("inactiveFrame",
                                                      &colors[Frame]);
    colors[TitleBar+KWINCOLORS] = colors[TitleBar];
    colors[TitleBar+KWINCOLORS] = config->
        readColorEntry("inactiveBackground", &colors[TitleBar+KWINCOLORS]);

    if(QPixmap::defaultDepth() < 15)
        colors[TitleBlend+KWINCOLORS] = colors[ TitleBar+KWINCOLORS ];
    else
        colors[TitleBlend+KWINCOLORS] = colors[ TitleBar+KWINCOLORS ].dark(150);
    colors[TitleBlend+KWINCOLORS] =
        config->readColorEntry("inactiveBlend", &colors[TitleBlend+KWINCOLORS]);

    colors[ButtonBg+KWINCOLORS] = colors[Frame+KWINCOLORS];
    colors[ButtonBg+KWINCOLORS] =
        config->readColorEntry("buttonBackground",
                               &colors[ButtonBg]);

    if(QPixmap::defaultDepth() < 15)
        colors[ButtonBlend+KWINCOLORS] = colors[ ButtonBg+KWINCOLORS ];
    else
        colors[ButtonBlend+KWINCOLORS] = colors[ ButtonBg+KWINCOLORS ].dark(150);
    colors[ButtonBlend+KWINCOLORS] =
        config->readColorEntry("buttonBlend", &colors[ButtonBlend+KWINCOLORS]);

    colors[ButtonFg+KWINCOLORS] = config->
        readColorEntry("buttonForeground", &colors[ButtonFg]);

    colors[Handle+KWINCOLORS] =
        config->readColorEntry("inactiveHandle", &colors[Handle]);

    colors[Font+KWINCOLORS] = Qt::darkGray;
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
}
