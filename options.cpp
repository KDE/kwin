#include "options.h"
#include <qpalette.h>
#include <qapplication.h>
#include <kconfig.h>
#include <kglobal.h>

// increment this when you add a color type (mosfet)
#define KWINCOLORS 8
Options::Options(){
    reload();
}

const QColor& Options::color(ColorType type, bool active)
{
    return(colors[type + (active ? 0 : KWINCOLORS)]);
}

const QFont& Options::font(bool active)
{
    return(active ? activeFont : inactiveFont);
}


void Options::reload()
{
    focusPolicy = ClickToFocus;
    resizeMode = Opaque;
    moveMode = Transparent;//HalfTransparent;

    QPalette pal = QApplication::palette();
    KConfig *config = KGlobal::config();
    config->setGroup("WM");

    // normal colors
    colors[Frame] = Qt::lightGray;
    colors[Frame] = config->readColorEntry("frame", &colors[Frame]);
    colors[Handle] = config->readColorEntry("handle", &colors[Frame]);
    colors[ButtonBg] = config->readColorEntry("buttonBackground",
                                              &colors[Frame]);
    colors[ButtonBlend] = config->readColorEntry("buttonBlend",
                                                 &colors[ButtonBg]);
    colors[TitleBar] = Qt::darkBlue;
    colors[TitleBar] = config->readColorEntry("activeBackground",
                                              &colors[TitleBar]);
    colors[TitleBlend] = config->readColorEntry("activeBlend",
                                                &colors[TitleBar]);

    colors[Font] = Qt::white;
    colors[Font] = config->readColorEntry("activeForeground", &colors[Font]);
    colors[ButtonFg] = Qt::lightGray;
    colors[ButtonFg] = config->readColorEntry("buttonForeground",
                                              &colors[ButtonFg]);

    // inactive
    colors[Frame+KWINCOLORS] = colors[Frame];
    colors[Frame+KWINCOLORS] =
        config->readColorEntry("inactiveFrame", &colors[Frame+KWINCOLORS]);
    colors[Handle+KWINCOLORS] =
        config->readColorEntry("inactiveHandle", &colors[Frame+KWINCOLORS]);
    colors[TitleBar+KWINCOLORS] = Qt::darkGray;
    colors[TitleBar+KWINCOLORS] = config->
        readColorEntry("inactiveBackground", &colors[TitleBar+KWINCOLORS]);
    colors[TitleBlend+KWINCOLORS] =
        config->readColorEntry("inactiveBlend", &colors[TitleBar+KWINCOLORS]);
    colors[ButtonBg+KWINCOLORS] =
        config->readColorEntry("inactiveButtonBackground",
                               &colors[Frame+KWINCOLORS]);
    colors[ButtonBlend+KWINCOLORS] =
        config->readColorEntry("inactiveButtonBlend",
                               &colors[ButtonBg+KWINCOLORS]);

    colors[Font+KWINCOLORS] = Qt::lightGray;
    colors[Font+KWINCOLORS] = config->readColorEntry("inactiveForeground",
                                                     &colors[Font+KWINCOLORS]);
    colors[ButtonFg+KWINCOLORS] = config->
        readColorEntry("inactiveButtonForeground", &colors[ButtonFg]);

    activeFont = QFont("Helvetica", 12, QFont::Bold);
    activeFont = config->readFontEntry("activeFont", &activeFont);
    inactiveFont = config->readFontEntry("inactiveFont", &activeFont);
}
