/* Plastik KWin window decoration
  Copyright (C) 2003 Sandro Giessl <ceebx@users.sourceforge.net>

  based on the window decoration "Web":
  Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
 */

#include <qpainter.h>
#include <qimage.h>

#include <kconfig.h>
#include <kpixmap.h>
#include <kpixmapeffect.h>

#include "xpm/close.xpm"
#include "xpm/minimize.xpm"
#include "xpm/maximize.xpm"
#include "xpm/restore.xpm"
#include "xpm/help.xpm"
#include "xpm/sticky.xpm"
#include "xpm/unsticky.xpm"
#include "xpm/shade.xpm"
#include "xpm/unshade.xpm"
#include "xpm/keepabove.xpm"
#include "xpm/notkeepabove.xpm"
#include "xpm/keepbelow.xpm"
#include "xpm/notkeepbelow.xpm"
#include "xpm/empty.xpm"

#include "misc.h"
#include "plastik.h"
#include "plastik.moc"
#include "plastikclient.h"

namespace KWinPlastik
{

PlastikHandler::PlastikHandler()
{
    memset(m_pixmaps, 0, sizeof(QPixmap*)*(NumPixmaps+NumButtonPixmaps*2)*2*2); // set elements to 0

    reset(0);
}

PlastikHandler::~PlastikHandler()
{
    for (int t=0; t < 2; ++t)
        for (int a=0; a < 2; ++a)
            for (int i=0; i < NumPixmaps+NumButtonPixmaps*2; ++i)
                delete m_pixmaps[t][a][i];
}

bool PlastikHandler::reset(unsigned long changed)
{
    // we assume the active font to be the same as the inactive font since the control
    // center doesn't offer different settings anyways.
    m_titleFont = KDecoration::options()->font(true, false); // not small
    m_titleFontTool = KDecoration::options()->font(true, true); // small

    switch(KDecoration::options()->preferredBorderSize( this )) {
        case BorderTiny:
            m_borderSize = 2;
            break;
        case BorderLarge:
            m_borderSize = 8;
            break;
        case BorderVeryLarge:
            m_borderSize = 12;
            break;
        case BorderHuge:
            m_borderSize = 18;
            break;
        case BorderVeryHuge:
            m_borderSize = 27;
            break;
        case BorderOversized:
            m_borderSize = 40;
            break;
        case BorderNormal:
        default:
            m_borderSize = 4;
    }

    // check if we are in reverse layout mode
    m_reverse = QApplication::reverseLayout();

    // read in the configuration
    readConfig();

    // pixmaps probably need to be updated, so delete the cache.
    for (int t=0; t < 2; ++t) {
        for (int a=0; a < 2; ++a) {
            for (int i=0; i < NumPixmaps+NumButtonPixmaps*2; i++) {
                if (m_pixmaps[t][a][i]) {
                    delete m_pixmaps[t][a][i];
                    m_pixmaps[t][a][i] = 0;
                }
            }
        }
    }

    // Do we need to "hit the wooden hammer" ?
    bool needHardReset = true;
    // TODO: besides the Color and Font settings I can maybe handle more changes
    //       without a hard reset. I will do this later...
    if (changed & SettingColors || changed & SettingFont)
    {
        needHardReset = false;
    }

    if (needHardReset) {
        return true;
    } else {
        resetDecorations(changed);
        return false;
    }
}

KDecoration* PlastikHandler::createDecoration( KDecorationBridge* bridge )
{
        return new PlastikClient( bridge, this );
}

bool PlastikHandler::supports( Ability ability )
{
    switch( ability )
    {
        case AbilityAnnounceButtons:
        case AbilityButtonMenu:
        case AbilityButtonOnAllDesktops:
        case AbilityButtonSpacer:
        case AbilityButtonHelp:
        case AbilityButtonMinimize:
        case AbilityButtonMaximize:
        case AbilityButtonClose:
        case AbilityButtonAboveOthers:
        case AbilityButtonBelowOthers:
        case AbilityButtonShade:
            return true;
        default:
            return false;
    };
}

void PlastikHandler::readConfig()
{
    // create a config object
    KConfig config("kwinplastikrc");
    config.setGroup("General");

    // grab settings
    m_titleShadow    = config.readBoolEntry("TitleShadow", true);

    QFontMetrics fm(m_titleFont);  // active font = inactive font
    int titleHeightMin = config.readNumEntry("MinTitleHeight", 16);
    // The title should strech with bigger font sizes!
    m_titleHeight = QMAX(titleHeightMin, fm.height() + 4); // 4 px for the shadow etc.

    fm = QFontMetrics(m_titleFontTool);  // active font = inactive font
    int titleHeightToolMin = config.readNumEntry("MinTitleHeightTool", 13);
    // The title should strech with bigger font sizes!
    m_titleHeightTool = QMAX(titleHeightToolMin, fm.height() ); // don't care about the shadow etc.

    QString value = config.readEntry("TitleAlignment", "AlignHCenter");
    if (value == "AlignLeft")         m_titleAlign = Qt::AlignLeft;
    else if (value == "AlignHCenter") m_titleAlign = Qt::AlignHCenter;
    else if (value == "AlignRight")   m_titleAlign = Qt::AlignRight;

    m_animateButtons = config.readBoolEntry("AnimateButtons", true);
    m_menuClose = config.readBoolEntry("CloseOnMenuDoubleClick", true);
}

QColor PlastikHandler::getColor(KWinPlastik::ColorType type, const bool active)
{
    switch (type) {
        case WindowContour:
            return KDecoration::options()->color(ColorTitleBar, active).dark(190);
        case TitleGradient1:
            return hsvRelative(KDecoration::options()->color(ColorTitleBar, active), 0,-10,+10);
            break;
        case TitleGradient2:
            return hsvRelative(KDecoration::options()->color(ColorTitleBar, active), 0,0,-25);
            break;
        case TitleGradient3:
            return KDecoration::options()->color(ColorTitleBar, active);
            break;
        case TitleHighlightTop:
        case SideHighlightLeft:
            return alphaBlendColors(KDecoration::options()->color(ColorTitleBar, active),
                    Qt::white, active?150:160);
            break;
        case SideHighlightRight:
        case SideHighlightBottom:
            return alphaBlendColors(KDecoration::options()->color(ColorTitleBar, active),
                    Qt::black, active?150:160);
            break;
        case Border:
            return KDecoration::options()->color(ColorFrame, active);
        case TitleFont:
            return KDecoration::options()->color(ColorFont, active);
        default:
            return Qt::black;
    }
}

const QPixmap &PlastikHandler::pixmap(Pixmaps type, bool active, bool toolWindow)
{
    if (m_pixmaps[toolWindow][active][type])
        return *m_pixmaps[toolWindow][active][type];

    switch (type) {
        case TitleBarTileTop:
        case TitleBarTile:
        {
            const int topHeight = 2;
            const int topGradientHeight = 4;

            int h = (toolWindow ? m_titleHeightTool : m_titleHeight) + topHeight;

            QPixmap gradient(1, h); // TODO: test width of 5 or so for performance

            QPainter painter(&gradient);

            KPixmap tempPixmap;
            tempPixmap.resize(1, topGradientHeight);
            KPixmapEffect::gradient(tempPixmap,
                                    getColor(TitleGradient1, active),
                                    getColor(TitleGradient2, active),
                                    KPixmapEffect::VerticalGradient);
            painter.drawPixmap(0,0, tempPixmap);

            tempPixmap.resize(1, h-topGradientHeight);
            KPixmapEffect::gradient(tempPixmap,
                                    getColor(TitleGradient2, active),
                                    getColor(TitleGradient3, active),
                                    KPixmapEffect::VerticalGradient);
            painter.drawPixmap(0,topGradientHeight, tempPixmap);
            painter.end();

            QPixmap *pixmap;
            if (type == TitleBarTileTop) {
                pixmap = new QPixmap(1, topHeight);
                painter.begin(pixmap);
                painter.drawPixmap(0, 0, gradient);
                painter.end();
            } else {
                pixmap = new QPixmap(1, h-topHeight);
                painter.begin(pixmap);
                painter.drawPixmap(0, 0, gradient, 0,topHeight);
                painter.end();
            }

            m_pixmaps[toolWindow][active][type] = pixmap;
            return *pixmap;

            break;
        }

    }
}

const QPixmap &PlastikHandler::buttonPixmap(ButtonPixmaps type, const QSize &size, bool pressed, bool active, bool toolWindow)
{
    int typeIndex = NumPixmaps+(pressed?type*2:type);

    // btn icon size...
    int reduceW = 0, reduceH = 0;
    if(size.width()>12) {
        reduceW = static_cast<int>(2*(size.width()/3.5) );
    }
    else
        reduceW = 4;
    if(size.height()>12)
        reduceH = static_cast<int>(2*(size.height()/3.5) );
    else
        reduceH = 4;

    int w = size.width() - reduceW;
    int h = size.height() - reduceH;

    if (m_pixmaps[toolWindow][active][typeIndex] && m_pixmaps[toolWindow][active][typeIndex]->size()==QSize(w,h) )
        return *m_pixmaps[toolWindow][active][typeIndex];

    // no matching pixmap found, create a new one...

    delete m_pixmaps[toolWindow][active][typeIndex];
    m_pixmaps[toolWindow][active][typeIndex] = 0;

    QColor iconColor = alphaBlendColors(getColor(TitleGradient3, active), pressed ? Qt::white : Qt::black, 50);

    QImage img;

    switch (type) {
        case BtnHelp:
            img = QImage(help_xpm);
            break;
        case BtnMax:
            img = QImage(maximize_xpm);
            break;
        case BtnMaxRestore:
            img = QImage(restore_xpm);
            break;
        case BtnMin:
            img = QImage(minimize_xpm);
            break;
        case BtnClose:
            img = QImage(close_xpm);
            break;
        case BtnOnAllDesktops:
            img = QImage(sticky_xpm);
            break;
        case BtnNotOnAllDesktops:
            img = QImage(unsticky_xpm);
            break;
        case BtnAbove:
            img = QImage(keepabove_xpm);
            break;
        case BtnNotAbove:
            img = QImage(notkeepabove_xpm);
            break;
        case BtnBelow:
            img = QImage(keepbelow_xpm);
        case BtnNotBelow:
            img = QImage(notkeepbelow_xpm);
            break;
        case BtnShade:
            img = QImage(shade_xpm);
            break;
        case BtnShadeRestore:
            img = QImage(unshade_xpm);
            break;

        default:
            img = QImage(empty_xpm);
            break;
    }

    QPixmap *pixmap = new QPixmap(recolorImage(&img, iconColor).smoothScale(w,h) );
    m_pixmaps[toolWindow][active][typeIndex] = pixmap;
    return *pixmap;

}

QValueList< PlastikHandler::BorderSize >
PlastikHandler::borderSizes() const
{
    // the list must be sorted
    return QValueList< BorderSize >() << BorderTiny << BorderNormal <<
	BorderLarge << BorderVeryLarge <<  BorderHuge <<
	BorderVeryHuge << BorderOversized;
}

// make the handler accessible to other classes...
static PlastikHandler *handler = 0;
PlastikHandler* Handler()
{
    return handler;
}

} // KWinPlastik

//////////////////////////////////////////////////////////////////////////////
// Plugin Stuff                                                             //
//////////////////////////////////////////////////////////////////////////////

extern "C"
{
    KDE_EXPORT KDecorationFactory *create_factory()
    {
        KWinPlastik::handler = new KWinPlastik::PlastikHandler();
        return KWinPlastik::handler;
    }
}
