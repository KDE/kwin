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

#include <kconfig.h>
#include <kpixmap.h>
#include <kpixmapeffect.h>

#include "misc.h"
#include "plastik.h"
#include "plastik.moc"
#include "plastikclient.h"

namespace KWinPlastik
{

bool PlastikHandler::m_initialized    = false;
bool PlastikHandler::m_animateButtons = true;
bool PlastikHandler::m_titleShadow    = true;
bool PlastikHandler::m_shrinkBorders  = true;
bool PlastikHandler::m_menuClose      = false;
bool PlastikHandler::m_reverse        = false;
int  PlastikHandler::m_borderSize     = 4;
int  PlastikHandler::m_titleHeight    = 19;
int  PlastikHandler::m_titleHeightTool= 12;
QFont PlastikHandler::m_titleFont = QFont();
QFont PlastikHandler::m_titleFontTool = QFont();
Qt::AlignmentFlags PlastikHandler::m_titleAlign = Qt::AlignHCenter;

PlastikHandler::PlastikHandler()
{
    memset(m_pixmaps, 0, sizeof(QPixmap *) * NumPixmaps); // set elements to 0

    reset(0);
}

PlastikHandler::~PlastikHandler()
{
    m_initialized = false;

    for (int n=0; n<NumPixmaps; n++) {
        if (m_pixmaps[n]) delete m_pixmaps[n];
    }
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
    for (int n=0; n<NumPixmaps; n++) {
        if (m_pixmaps[n]) {
            delete m_pixmaps[n];
            m_pixmaps[n] = 0;
        }
    }

    m_initialized = true;

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
        case TitleGradientFrom:
            return KDecoration::options()->color(ColorTitleBar, active);
            break;
        case TitleGradientTo:
            return alphaBlendColors(KDecoration::options()->color(ColorTitleBar, active),
                    Qt::white, active?210:220);
            break;
        case TitleGradientToTop:
            return alphaBlendColors(KDecoration::options()->color(ColorTitleBar, active),
                    Qt::white, active?180:190);
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

const QPixmap &PlastikHandler::pixmap(Pixmaps type) {

    if (m_pixmaps[type])
        return *m_pixmaps[type];

    switch (type) {
        case aTitleBarTileTop:
        case iTitleBarTileTop:
        case atTitleBarTileTop:
        case itTitleBarTileTop:
        {
            int h = 4-2; // TODO: don't hardcode the height...

            bool active = false;
            if (type == aTitleBarTileTop || type == atTitleBarTileTop) {
                active = true;
            }

            KPixmap tempPixmap;
            tempPixmap.resize(1, h);
            KPixmapEffect::gradient(tempPixmap,
                                    getColor(TitleGradientToTop, active),
                                    getColor(TitleGradientFrom, active),
                                    KPixmapEffect::VerticalGradient);
            QPixmap *pixmap = new QPixmap(1, h);
            QPainter painter(pixmap);
            painter.drawPixmap(0, 0, tempPixmap);
            painter.end();

            m_pixmaps[type] = pixmap;
            return *pixmap;

            break;
        }

        case aTitleBarTile:
        case iTitleBarTile:
        case atTitleBarTile:
        case itTitleBarTile:
        {
            bool active = false;
            if (type == aTitleBarTile || type == atTitleBarTile) {
                active = true;
            }
            bool toolWindow = false;
            if (type == atTitleBarTile || type == itTitleBarTile) {
                toolWindow = true;
            }

            int h = toolWindow ? m_titleHeightTool : m_titleHeight;

            KPixmap tempPixmap;
            tempPixmap.resize(5, h);
            KPixmapEffect::gradient(tempPixmap,
                                    PlastikHandler::getColor(TitleGradientFrom, active),
                                    PlastikHandler::getColor(TitleGradientTo, active),
                                    KPixmapEffect::VerticalGradient);
            QPixmap *pixmap = new QPixmap(5, h);
            QPainter painter(pixmap);
            painter.drawPixmap(0, 0, tempPixmap);
            painter.end();

            m_pixmaps[type] = pixmap;
            return *pixmap;

            break;
        }

    }
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
