/* Plastik KWin window decoration
  Copyright (C) 2003-2005 Sandro Giessl <sandro@giessl.com>

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

#include <qbitmap.h>
#include <qpainter.h>
#include <qimage.h>

#include <kconfig.h>
#include <kpixmap.h>
#include <kpixmapeffect.h>

#include "misc.h"
#include "plastik.h"
#include "plastik.moc"
#include "plastikclient.h"
#include "plastikbutton.h"

namespace KWinPlastik
{

PlastikHandler::PlastikHandler()
{
    memset(m_pixmaps, 0, sizeof(QPixmap*)*NumPixmaps*2*2); // set elements to 0
    memset(m_bitmaps, 0, sizeof(QBitmap*)*NumButtonIcons*2);

    reset(0);
}

PlastikHandler::~PlastikHandler()
{
    for (int t=0; t < 2; ++t)
        for (int a=0; a < 2; ++a)
            for (int i=0; i < NumPixmaps; ++i)
                delete m_pixmaps[t][a][i];
    for (int t=0; t < 2; ++t)
        for (int i=0; i < NumButtonIcons; ++i)
            delete m_bitmaps[t][i];
}

bool PlastikHandler::reset(unsigned long changed)
{
    // we assume the active font to be the same as the inactive font since the control
    // center doesn't offer different settings anyways.
    m_titleFont = KDecoration::options()->font(true, false); // not small
    m_titleFontTool = KDecoration::options()->font(true, true); // small

    switch(KDecoration::options()->preferredBorderSize( this )) {
        case BorderTiny:
            m_borderSize = 3;
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
            for (int i=0; i < NumPixmaps; i++) {
                if (m_pixmaps[t][a][i]) {
                    delete m_pixmaps[t][a][i];
                    m_pixmaps[t][a][i] = 0;
                }
            }
        }
    }
    for (int t=0; t < 2; ++t) {
        for (int i=0; i < NumButtonIcons; i++) {
            if (m_bitmaps[t][i]) {
                delete m_bitmaps[t][i];
                m_bitmaps[t][i] = 0;
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
    } else if (changed & SettingButtons) {
        // handled by KCommonDecoration
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
    // have an even title/button size so the button icons are fully centered...
    if ( m_titleHeight%2 == 0)
        m_titleHeight++;

    fm = QFontMetrics(m_titleFontTool);  // active font = inactive font
    int titleHeightToolMin = config.readNumEntry("MinTitleHeightTool", 13);
    // The title should strech with bigger font sizes!
    m_titleHeightTool = QMAX(titleHeightToolMin, fm.height() ); // don't care about the shadow etc.
    // have an even title/button size so the button icons are fully centered...
    if ( m_titleHeightTool%2 == 0)
        m_titleHeightTool++;

    QString value = config.readEntry("TitleAlignment", "AlignHCenter");
    if (value == "AlignLeft")         m_titleAlign = Qt::AlignLeft;
    else if (value == "AlignHCenter") m_titleAlign = Qt::AlignHCenter;
    else if (value == "AlignRight")   m_titleAlign = Qt::AlignRight;

    m_coloredBorder = config.readBoolEntry("ColoredBorder", true);
    m_animateButtons = config.readBoolEntry("AnimateButtons", true);
    m_menuClose = config.readBoolEntry("CloseOnMenuDoubleClick", true);
}

QColor PlastikHandler::getColor(KWinPlastik::ColorType type, const bool active)
{
    switch (type) {
        case WindowContour:
            return KDecoration::options()->color(ColorTitleBar, active).dark(200);
        case TitleGradient1:
            return hsvRelative(KDecoration::options()->color(ColorTitleBar, active), 0,-10,+10);
            break;
        case TitleGradient2:
            return hsvRelative(KDecoration::options()->color(ColorTitleBar, active), 0,0,-25);
            break;
        case TitleGradient3:
            return KDecoration::options()->color(ColorTitleBar, active);
            break;
        case ShadeTitleLight:
            return alphaBlendColors(KDecoration::options()->color(ColorTitleBar, active),
                                    Qt::white, active?205:215);
            break;
        case ShadeTitleDark:
            return alphaBlendColors(KDecoration::options()->color(ColorTitleBar, active),
                                    Qt::black, active?205:215);
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

    QPixmap *pm = 0;

    switch (type) {
        case TitleBarTileTop:
        case TitleBarTile:
        {
            const int titleBarTileHeight = (toolWindow ? m_titleHeightTool : m_titleHeight) + 2;
            // gradient used as well in TitleBarTileTop as TitleBarTile
            const int gradientHeight = 2 + titleBarTileHeight-1;
            QPixmap gradient(1, gradientHeight);
            QPainter painter(&gradient);
            KPixmap tempPixmap;
            tempPixmap.resize(1, 4);
            KPixmapEffect::gradient(tempPixmap,
                                    getColor(TitleGradient1, active),
                                    getColor(TitleGradient2, active),
                                    KPixmapEffect::VerticalGradient);
            painter.drawPixmap(0,0, tempPixmap);
            tempPixmap.resize(1, gradientHeight-4);
            KPixmapEffect::gradient(tempPixmap,
                                    getColor(TitleGradient2, active),
                                    getColor(TitleGradient3, active),
                                    KPixmapEffect::VerticalGradient);
            painter.drawPixmap(0,4, tempPixmap);
            painter.end();

            // actual titlebar tiles
            if (type == TitleBarTileTop) {
                pm = new QPixmap(1, 4);
                painter.begin(pm);
                // contour
                painter.setPen(getColor(WindowContour, active) );
                painter.drawPoint(0,0);
                // top highlight
                painter.setPen(getColor(ShadeTitleLight, active) );
                painter.drawPoint(0,1);
                // gradient
                painter.drawPixmap(0, 2, gradient);
                painter.end();
            } else {
                pm = new QPixmap(1, titleBarTileHeight);
                painter.begin(pm);
                painter.drawPixmap(0, 0, gradient, 0,2);
                if (m_coloredBorder) {
                    painter.setPen(getColor(TitleGradient3, active).dark(110) );
                } else {
                    painter.setPen(getColor(TitleGradient3, active) );
                }
                painter.drawPoint(0,titleBarTileHeight-1);
                painter.end();
            }

            break;
        }

        case TitleBarLeft:
        {
            const int w = m_borderSize;
            const int h = 4 + (toolWindow ? m_titleHeightTool : m_titleHeight) + 2;

            pm = new QPixmap(w, h);
            QPainter painter(pm);

            painter.drawTiledPixmap(0,0, w, 4, pixmap(TitleBarTileTop, active, toolWindow) );
            painter.drawTiledPixmap(0,4, w, h-4, pixmap(TitleBarTile, active, toolWindow) );

            painter.setPen(getColor(WindowContour, active) );
            painter.drawLine(0,0, 0,h);
            painter.drawPoint(1,1);

            const QColor highlightTitleLeft = getColor(ShadeTitleLight, active);
            painter.setPen(highlightTitleLeft);
            painter.drawLine(1,2, 1,h);

            if (m_coloredBorder) {
                painter.setPen(getColor(TitleGradient3, active) );
                painter.drawLine(2,h-1, w-1,h-1);
            }

            // outside the region normally masked by doShape
            painter.setPen(QColor(0,0,0) );
            painter.drawLine(0, 0, 1, 0 );
            painter.drawPoint(0, 1);

            break;
        }

        case TitleBarRight:
        {
            const int w = m_borderSize;
            const int h = 4 + (toolWindow ? m_titleHeightTool : m_titleHeight) + 2;

            pm = new QPixmap(w, h);
            QPainter painter(pm);

            painter.drawTiledPixmap(0,0, w, 4, pixmap(TitleBarTileTop, active, toolWindow) );
            painter.drawTiledPixmap(0,4, w, h-4, pixmap(TitleBarTile, active, toolWindow) );

            painter.setPen(getColor(WindowContour, active) );
            painter.drawLine(w-1,0, w-1,h);
            painter.drawPoint(w-2,1);

            const QColor highlightTitleRight = getColor(ShadeTitleDark, active);
            painter.setPen(highlightTitleRight);
            painter.drawLine(w-2,2, w-2,h);

            if (m_coloredBorder) {
                painter.setPen(getColor(TitleGradient3, active) );
                painter.drawLine(0,h-1, w-3,h-1);
            }

            // outside the region normally masked by doShape
            painter.setPen(QColor(0,0,0) );
            painter.drawLine(w-2, 0, w-1, 0 );
            painter.drawPoint(w-1, 1);

            break;
        }

        case BorderLeftTile:
        {
            const int w = m_borderSize;

            pm = new QPixmap(w, 1);
            QPainter painter(pm);
            if (m_coloredBorder) {
                painter.setPen(getColor(WindowContour, active) );
                painter.drawPoint(0, 0);
                painter.setPen(getColor(ShadeTitleLight, active) );
                painter.drawPoint(1, 0);
                if (w > 3) {
                    painter.setPen(getColor(TitleGradient3, active) );
                    painter.drawLine(2,0, w-2,0);
                }
                painter.setPen(getColor(TitleGradient3, active).dark(110) );
                painter.drawPoint(w-1,0);
            } else {
                painter.setPen(getColor(WindowContour, active) );
                painter.drawPoint(0, 0);
                painter.setPen(
                        alphaBlendColors(getColor(Border, active),
                                         getColor(ShadeTitleLight, active), 130) );
                painter.drawPoint(1, 0);
                painter.setPen(getColor(Border, active) );
                painter.drawLine(2,0, w-1,0);
            }

            painter.end();

            break;
        }

        case BorderRightTile:
        {
            const int w = m_borderSize;

            pm = new QPixmap(w, 1);
            QPainter painter(pm);
            if (m_coloredBorder) {
                painter.setPen(getColor(TitleGradient3, active).dark(110) );
                painter.drawPoint(0,0);
                if (w > 3) {
                    painter.setPen(getColor(TitleGradient3, active) );
                    painter.drawLine(1,0, w-3,0);
                }
                painter.setPen(getColor(ShadeTitleDark, active) );
                painter.drawPoint(w-2, 0);
                painter.setPen(getColor(WindowContour, active) );
                painter.drawPoint(w-1, 0);
            } else {
                painter.setPen(getColor(Border, active) );
                painter.drawLine(0,0, w-3,0);
                painter.setPen(
                        alphaBlendColors(getColor(Border, active),
                                         getColor(ShadeTitleDark, active), 130) );
                painter.drawPoint(w-2, 0);
                painter.setPen(getColor(WindowContour, active) );
                painter.drawPoint(w-1, 0);
            }
            painter.end();

            break;
        }

        case BorderBottomLeft:
        {
            const int w = m_borderSize;
            const int h = m_borderSize;

            pm = new QPixmap(w, h);
            QPainter painter(pm);
            painter.drawTiledPixmap(0,0,w,h, pixmap(BorderBottomTile, active, toolWindow) );
            painter.setPen(getColor(WindowContour, active) );
            painter.drawLine(0,0, 0,h);
            if (m_coloredBorder) {
                if (h > 3) {
                    painter.setPen(getColor(ShadeTitleLight, active) );
                    painter.drawLine(1,0, 1,h-2);
                }

                painter.setPen(getColor(TitleGradient3, active) );
                painter.drawLine(2,0, w-1,0);
            } else {
                painter.setPen(
                        alphaBlendColors(getColor(Border, active),
                                        getColor(ShadeTitleLight, active), 130) );
                painter.drawLine(1,0, 1,h-2);
            }

            painter.end();

            break;
        }

        case BorderBottomRight:
        {
            const int w = m_borderSize;
            const int h = m_borderSize;

            pm = new QPixmap(w, h);
            QPainter painter(pm);
            painter.drawTiledPixmap(0,0,w,h, pixmap(BorderBottomTile, active, toolWindow) );
            painter.setPen(getColor(WindowContour, active) );
            painter.drawLine(w-1,0, w-1,h);
            if (m_coloredBorder) {
                painter.setPen(getColor(ShadeTitleDark, active) );
                painter.drawLine(w-2,0, w-2,h-2);

                painter.setPen(getColor(TitleGradient3, active) );
                painter.drawLine(0,0, w-3,0);
            } else {
                painter.setPen(
                        alphaBlendColors(getColor(Border, active),
                                         getColor(ShadeTitleDark, active), 130) );
                painter.drawLine(w-2,0, w-2,h-2);
            }

            painter.end();

            break;
        }

        case BorderBottomTile:
        default:
        {
            const int h = m_borderSize;

            pm = new QPixmap(1, m_borderSize);
            QPainter painter(pm);

            if (m_coloredBorder) {
                painter.setPen(getColor(TitleGradient3, active).dark(110) );
                painter.drawPoint(0,0);
                painter.setPen(getColor(TitleGradient3, active) );
                painter.drawLine(0,1, 0,h-3);
                painter.setPen(getColor(ShadeTitleDark, active) );
                painter.drawPoint(0, h-2);
            } else {
                painter.setPen(getColor(Border, active) );
                painter.drawLine(0,0, 0,h-3);
                painter.setPen(
                        alphaBlendColors(getColor(Border, active),
                                        getColor(ShadeTitleDark, active), 130) );
                painter.drawPoint(0, h-2);
            }
            painter.setPen(getColor(WindowContour, active) );
            painter.drawPoint(0, h-1);
            painter.end();

            break;
        }
    }

    m_pixmaps[toolWindow][active][type] = pm;
    return *pm;
}

const QBitmap &PlastikHandler::buttonBitmap(ButtonIcon type, const QSize &size, bool toolWindow)
{
    int typeIndex = NumPixmaps+type;

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

    if (m_bitmaps[toolWindow][typeIndex] && m_bitmaps[toolWindow][typeIndex]->size()==QSize(w,h) )
        return *m_bitmaps[toolWindow][typeIndex];

    // no matching pixmap found, create a new one...

    delete m_bitmaps[toolWindow][typeIndex];
    m_bitmaps[toolWindow][typeIndex] = 0;

    QBitmap bmp = IconEngine::icon(type /*icon*/, QMIN(w,h) );
    QBitmap *bitmap = new QBitmap(bmp);
    m_bitmaps[toolWindow][typeIndex] = bitmap;
    return *bitmap;
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
