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

#include <klocale.h>
#include <kpixmap.h>
#include <kpixmapeffect.h>
#include <kapplication.h>

#include <qbitmap.h>
#include <qdatetime.h>
#include <qfontmetrics.h>
#include <qimage.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qdesktopwidget.h>

#include "plastikclient.h"
#include "plastikbutton.h"
#include "misc.h"
#include "shadow.h"

namespace KWinPlastik
{

PlastikClient::PlastikClient(KDecorationBridge* bridge, KDecorationFactory* factory)
    : KCommonDecoration (bridge, factory),
    s_titleFont(QFont() )
{
    memset(m_captionPixmaps, 0, sizeof(QPixmap*)*2);
}

PlastikClient::~PlastikClient()
{
    clearCaptionPixmaps();
}

QString PlastikClient::visibleName() const
{
    return i18n("Plastik");
}

QString PlastikClient::defaultButtonsLeft() const
{
    return "M";
}

QString PlastikClient::defaultButtonsRight() const
{
    return "HIAX";
}

bool PlastikClient::decorationBehaviour(DecorationBehaviour behaviour) const
{
    switch (behaviour) {
        case DB_MenuClose:
            return Handler()->menuClose();

        case DB_WindowMask:
            return true;

        default:
            return KCommonDecoration::decorationBehaviour(behaviour);
    }
}

int PlastikClient::layoutMetric(LayoutMetric lm, bool respectWindowState, const KCommonDecorationButton *) const
{
    bool maximized = maximizeMode()==MaximizeFull && !options()->moveResizeMaximizedWindows();

    switch (lm) {
        case LM_BorderLeft:
        case LM_BorderRight:
        case LM_BorderBottom:
        {
            if (respectWindowState && maximized) {
                return 0;
            } else {
                return Handler()->borderSize();
            }
        }

        case LM_TitleEdgeTop:
        {
            if (respectWindowState && maximized) {
                return 0;
            } else {
                return 4;
            }
        }

        case LM_TitleEdgeBottom:
        {
            if (respectWindowState && maximized) {
                return 1;
            } else {
                return 2;
            }
        }

        case LM_TitleEdgeLeft:
        case LM_TitleEdgeRight:
        {
            if (respectWindowState && maximized) {
                return 0;
            } else {
                return 6;
            }
        }

        case LM_TitleBorderLeft:
        case LM_TitleBorderRight:
            return 5;

        case LM_ButtonWidth:
        case LM_ButtonHeight:
        case LM_TitleHeight:
        {
            if (respectWindowState && isToolWindow()) {
                return Handler()->titleHeightTool();
            } else {
                return Handler()->titleHeight();
            }
        }

        case LM_ButtonSpacing:
            return 1;

        case LM_ExplicitButtonSpacer:
            return 3;

        default:
            return 0;
    }
}

KCommonDecorationButton *PlastikClient::createButton(ButtonType type)
{
    switch (type) {
        case MenuButton:
            return new PlastikButton(MenuButton, this, "menu");

        case OnAllDesktopsButton:
            return new PlastikButton(OnAllDesktopsButton, this, "on_all_desktops");

        case HelpButton:
            return new PlastikButton(HelpButton, this, "help");

        case MinButton:
            return new PlastikButton(MinButton, this, "minimize");

        case MaxButton:
            return new PlastikButton(MaxButton, this, "maximize");

        case CloseButton:
            return new PlastikButton(CloseButton, this, "close");

        case AboveButton:
            return new PlastikButton(AboveButton, this, "above");

        case BelowButton:
            return new PlastikButton(BelowButton, this, "below");

        case ShadeButton:
            return new PlastikButton(ShadeButton, this, "shade");

        default:
            return 0;
    }
}

void PlastikClient::init()
{
    s_titleFont = isToolWindow() ? Handler()->titleFontTool() : Handler()->titleFont();

    clearCaptionPixmaps();

    KCommonDecoration::init();
}

QRegion PlastikClient::cornerShape(WindowCorner corner)
{
    int w = widget()->width();
    int h = widget()->height();

    switch (corner) {
        case WC_TopLeft:
            if (layoutMetric(LM_TitleEdgeLeft) > 0)
                return QRegion(0, 0, 1, 2) + QRegion(1, 0, 1, 1);
            else
                return QRegion();

        case WC_TopRight:
            if (layoutMetric(LM_TitleEdgeRight) > 0)
                return QRegion(w-1, 0, 1, 2) + QRegion(w-2, 0, 1, 1);
            else
                return QRegion();

        case WC_BottomLeft:
            if (layoutMetric(LM_BorderBottom) > 0)
                return QRegion(0, h-1, 1, 1);
            else
                return QRegion();

        case WC_BottomRight:
            if (layoutMetric(LM_BorderBottom) > 0)
                return QRegion(w-1, h-1, 1, 1);
            else
                return QRegion();

        default:
            return QRegion();
    }

}

void PlastikClient::paintEvent(QPaintEvent *e)
{
    QRegion region = e->region();

    PlastikHandler *handler = Handler();

    if (oldCaption != caption() )
        clearCaptionPixmaps();

    bool active = isActive();
    bool toolWindow = isToolWindow();

    QPainter painter(widget() );

    QRegion mask;

    // colors...
    const QColor windowContour = Handler()->getColor(WindowContour, active);
    const QColor deco = Handler()->getColor(TitleGradient3, active);
    const QColor border = Handler()->getColor(Border, active);
    const QColor highlightTop = Handler()->getColor(TitleHighlightTop, active);
    const QColor highlightTitleLeft = alphaBlendColors(deco,
            Handler()->getColor(SideHighlightLeft, active), 150);
    const QColor highlightTitleRight = alphaBlendColors(deco,
            Handler()->getColor(SideHighlightRight, active), 150);
    const QColor highlightLeft = alphaBlendColors(border,
            Handler()->getColor(SideHighlightLeft, active), 150);
    const QColor highlightRight = alphaBlendColors(border,
            Handler()->getColor(SideHighlightRight, active), 150);
    const QColor highlightBottom = alphaBlendColors(border,
            Handler()->getColor(SideHighlightBottom, active), 150);
    const QColor filledCorner = QColor(0,0,0);



    // often needed coordinates
    QRect r = widget()->rect();

    int r_w = r.width();
//     int r_h = r.height();
    int r_x, r_y, r_x2, r_y2;
    r.coords(&r_x, &r_y, &r_x2, &r_y2);
    const int borderLeft = layoutMetric(LM_BorderLeft);
    const int borderRight = layoutMetric(LM_BorderRight);
    const int borderBottom = layoutMetric(LM_BorderBottom);
    const int titleHeight = layoutMetric(LM_TitleHeight);
    const int titleEdgeTop = layoutMetric(LM_TitleEdgeTop);
    const int titleEdgeBottom = layoutMetric(LM_TitleEdgeBottom);
    const int titleEdgeLeft = layoutMetric(LM_TitleEdgeLeft);
    const int titleEdgeRight = layoutMetric(LM_TitleEdgeRight);

    const int borderBottomTop = r_y2-borderBottom+1;
    const int borderLeftRight = r_x+borderLeft-1;
    const int borderRightLeft = r_x2-borderRight+1;
    const int titleEdgeLeftRight = r_x+titleEdgeLeft-1;
    const int titleEdgeRightLeft = r_x2-titleEdgeRight+1;
    const int titleEdgeTopBottom = r_y+titleEdgeTop;
    const int titleEdgeBottomTop = r_y+titleEdgeTop+titleHeight+1;
    const int titleEdgeBottomBottom = r_y+titleEdgeTop+titleHeight+titleEdgeBottom-1;

    const int sideHeight = borderBottomTop-titleEdgeBottomBottom-1;

    QRect Rtitle = QRect(r_x+titleEdgeLeft+buttonsLeftWidth(), r_y+titleEdgeTop,
                         r_x2-titleEdgeRight-buttonsRightWidth()-(r_x+titleEdgeLeft+buttonsLeftWidth()),
                         titleEdgeBottomBottom-(r_y+titleEdgeTop) );

    QRect tempRect;

    // Draw the window contour
    painter.setPen(windowContour );
    if (titleEdgeTop > 0) {
        painter.drawLine(r_x+2, r_y, r_x2-2, r_y );
        painter.drawPoint(r_x+1, r_y+1 );
        painter.drawPoint(r_x2-1, r_y+1 );
        painter.drawLine(r_x, r_y+2, r_x, titleEdgeTopBottom );
        painter.drawLine(r_x2, r_y+2, r_x2, titleEdgeTopBottom );
    }
    if(borderLeft > 0 && sideHeight > 0) {
        painter.drawLine(r_x, Rtitle.top()/*titleEdgeBottomBottom+1*/,
                         r_x, r_y2-1 );
    }
    if(borderRight > 0 && sideHeight > 0) {
        painter.drawLine(r_x2, Rtitle.top()/*titleEdgeBottomBottom+1*/,
                         r_x2, r_y2-1 );
    }
    if(borderBottom > 0) {
        painter.drawLine(r_x+1, r_y2,
                         r_x2-1, r_y2 );
    }

    // topSpacer
    if(titleEdgeTop > 0)
    {
        painter.setPen(highlightTop );
        painter.drawLine(r_x+3, r_y+1, r_x2-3, r_y+1 );
        // a bit anti-aliasing for the window contour...
        painter.setPen(alphaBlendColors(highlightTop, windowContour, 150) );
        painter.drawPoint(r_x+2, r_y+1);
        painter.drawPoint(r_x2-2, r_y+1);
        painter.setPen(alphaBlendColors(highlightTitleLeft, windowContour, 150) );
        painter.drawPoint(r_x+1, r_y+2);
        painter.setPen(alphaBlendColors(highlightTitleRight, windowContour, 150) );
        painter.drawPoint(r_x2-1, r_y+2);
        // highlight...
        painter.setPen(highlightTitleLeft);
        painter.drawLine(r_x+1, r_y+3, r_x+1, titleEdgeTopBottom );
        painter.setPen(highlightTitleRight);
        painter.drawLine(r_x2-1, r_y+3, r_x2-1, titleEdgeTopBottom );

        tempRect.setRect(r_x+2, r_y+2, r_w-2*2, titleEdgeTop-2 );
        if (tempRect.isValid() && region.contains(tempRect) ) {
            painter.drawTiledPixmap(tempRect, handler->pixmap(TitleBarTileTop, active, toolWindow) );
        }

        // outside the region normally masked by doShape
        painter.setPen(filledCorner);
        painter.drawLine(r_x, r_y, r_x+1, r_y );
        painter.drawPoint(r_x, r_y+1);
        painter.drawLine(r_x2, r_y, r_x2-1, r_y );
        painter.drawPoint(r_x2, r_y+1);
    }

    // leftTitleSpacer
    int titleMarginLeft = 0;
    int titleMarginRight = 0;
    if(titleEdgeLeft > 0)
    {
        painter.setPen(highlightTitleLeft);
        painter.drawLine(r_x+1, Rtitle.top(),
                         r_x+1, Rtitle.bottom() );

        titleMarginLeft = 2;
    }

    // rightTitleSpacer
    if(titleEdgeRight > 0)
    {
        painter.setPen(highlightTitleRight);
        painter.drawLine(r_x2-1, Rtitle.top(),
                         r_x2-1, Rtitle.bottom() );

        titleMarginRight = 2;
    }

    // titleSpacer
    const QPixmap &caption = captionPixmap();
//     QPixmap *titleBfrPtr = active ? aCaptionBuffer : iCaptionBuffer;
    if(Rtitle.width() > 0)
    {
        m_captionRect = captionRect(); // also update m_captionRect!
        if (m_captionRect.isValid() && region.contains(m_captionRect) )
        {
            painter.drawTiledPixmap(m_captionRect, caption);
        }

        // left to the title
        tempRect.setRect(r_x+titleMarginLeft, m_captionRect.top(),
                         m_captionRect.left() - (r_x+titleMarginLeft), m_captionRect.height() );
        if (tempRect.isValid() && region.contains(tempRect) ) {
            painter.drawTiledPixmap(tempRect, handler->pixmap(TitleBarTile, active, toolWindow) );
        }

        // right to the title
        tempRect.setRect(m_captionRect.right()+1, m_captionRect.top(),
                         (r_x2-titleMarginRight) - m_captionRect.right(), m_captionRect.height() );
        if (tempRect.isValid() && region.contains(tempRect) ) {
            painter.drawTiledPixmap(tempRect, handler->pixmap(TitleBarTile, active, toolWindow) );
        }

    }
//     titleBfrPtr = 0;

    // decoSpacer
    if(titleEdgeBottom > 0)
    {
        int l;
        if(borderLeft != 0)
            l = r_x+2;
        else
            l = r_x;
        int r;
        if(borderRight != 0)
            r = r_x2-2;
        else
            r = r_x2;

        painter.setPen(deco );
        painter.drawLine(l, titleEdgeBottomBottom-1, r, titleEdgeBottomBottom-1 );
        painter.drawLine(l, titleEdgeBottomBottom, r, titleEdgeBottomBottom );
        if(borderLeft != 0) {
            painter.setPen(highlightTitleLeft);
            painter.drawLine(r_x+1, titleEdgeBottomTop,
                            r_x+1, titleEdgeBottomBottom );
        }
        if(borderRight != 0) {
        painter.setPen(highlightTitleRight);
        painter.drawLine(r_x2-1, titleEdgeBottomTop,
                         r_x2-1, titleEdgeBottomBottom );
        }
    }

    // leftSpacer
    if(borderLeft > 0 && sideHeight > 0)
    {
        painter.setPen(highlightLeft );
        painter.drawLine(r_x+1, titleEdgeBottomBottom+1,
                            r_x+1, borderBottomTop-1 );
        if(borderLeft > 2) {
            tempRect.setCoords(r_x+2, titleEdgeBottomBottom+1,
                            borderLeftRight, borderBottomTop-1 );
            painter.fillRect(tempRect, border );
        }
    }

    // rightSpacer
    if(borderRight > 0 && sideHeight > 0)
    {
        painter.setPen(highlightRight );
        painter.drawLine(r_x2-1, titleEdgeBottomBottom+1,
                         r_x2-1, borderBottomTop-1 );
        if(borderRight > 2) {
            tempRect.setCoords(borderRightLeft, titleEdgeBottomBottom+1,
                               r_x2-2, borderBottomTop-1 );
            painter.fillRect(tempRect, border );
        }
    }

    // bottomSpacer
    if(borderBottom > 0)
    {
        if(borderLeft != 0) {
            painter.setPen(highlightLeft );
            painter.drawLine(r_x+1, borderBottomTop,
                            r_x+1, r_y2-2 );
            // anti-alias for the window contour...
            painter.setPen(alphaBlendColors(border, windowContour, 110) );
            painter.drawPoint(r_x+1, r_y2-1);
        }
        if(borderRight != 0) {
            painter.setPen(highlightRight );
            painter.drawLine(r_x2-1, borderBottomTop,
                            r_x2-1, r_y2-2 );
            // anti-alias for the window contour...
            painter.setPen(alphaBlendColors(border, windowContour, 110) );
            painter.drawPoint(r_x2-1, r_y2-1);
        }
        // outside the region normally masked by doShape
        painter.setPen(filledCorner);
        painter.drawPoint(r_x, r_y2);
        painter.drawPoint(r_x2, r_y2);

        int l;
        if(borderLeft != 0)
            l = r_x+2;
        else
            l = r_x;
        int r;
        if(borderRight != 0)
            r = r_x2-2;
        else
            r = r_x2;

        painter.setPen(highlightBottom );
        painter.drawLine(l, r_y2-1,
                         r, r_y2-1 );

        tempRect.setCoords(l, borderBottomTop, r, r_y2-2);
        painter.fillRect(tempRect, border );
    }
}

QRect PlastikClient::captionRect() const
{
    const QPixmap &caption = captionPixmap();
    QRect r = widget()->rect();

    const int titleHeight = layoutMetric(LM_TitleHeight);
    const int titleEdgeTop = layoutMetric(LM_TitleEdgeTop);
    const int titleEdgeLeft = layoutMetric(LM_TitleEdgeLeft);
    const int marginLeft = layoutMetric(LM_TitleBorderLeft);
    const int marginRight = layoutMetric(LM_TitleBorderRight);

    const int titleLeft = r.left() + titleEdgeLeft + buttonsLeftWidth() + marginLeft;
    const int titleWidth = r.width() -
            titleEdgeLeft - layoutMetric(LM_TitleEdgeRight) -
            buttonsLeftWidth() - buttonsRightWidth() -
            marginLeft - marginRight;

    Qt::AlignmentFlags a = Handler()->titleAlign();

    int tX, tW; // position/width of the title buffer
    if (caption.width() >  titleWidth) {
        tW = titleWidth;
    } else {
        tW = caption.width();
    }
    if (a == Qt::AlignLeft || (caption.width() > titleWidth) ) {
        // Align left
        tX = titleLeft;
    } else if (a == Qt::AlignHCenter) {
        // Align center
        tX = titleLeft+(titleWidth- caption.width() )/2;
    } else {
        // Align right
        tX = titleLeft+titleWidth-caption.width();
    }

    return QRect(tX, r.top()+titleEdgeTop, tW, titleHeight);
}

void PlastikClient::updateCaption()
{
    QRect oldCaptionRect = m_captionRect;

    if (oldCaption != caption() )
        clearCaptionPixmaps();

    m_captionRect = PlastikClient::captionRect();

    if (oldCaptionRect.isValid() && m_captionRect.isValid() )
        widget()->update(oldCaptionRect|m_captionRect);
    else
        widget()->update();
}

void PlastikClient::reset( unsigned long changed )
{
    if (changed & SettingColors)
    {
        // repaint the whole thing
        clearCaptionPixmaps();
        widget()->update();
        updateButtons();
    } else if (changed & SettingFont) {
        // font has changed -- update title height and font
        s_titleFont = isToolWindow() ? Handler()->titleFontTool() : Handler()->titleFont();

        updateLayout();

        // then repaint
        clearCaptionPixmaps();
        widget()->update();
    }

    KCommonDecoration::reset(changed);
}

const QPixmap &PlastikClient::getTitleBarTile(bool active) const
{
    return Handler()->pixmap(TitleBarTile, active, isToolWindow() );
}

const QPixmap &PlastikClient::captionPixmap() const
{
    bool active = isActive();

    if (m_captionPixmaps[active]) {
        return *m_captionPixmaps[active];
    }

    // not found, create new pixmap...

    const uint maxCaptionLength = 300; // truncate captions longer than this!
    QString c(caption() );
    if (c.length() > maxCaptionLength) {
        c.truncate(maxCaptionLength);
        c.append(" [...]");
    }

    QFontMetrics fm(s_titleFont);
    int captionWidth  = fm.width(c);
    int captionHeight = fm.height();

    const int th  = layoutMetric(LM_TitleHeight, false);

    QPainter painter;

    const int thickness = 2;
    QPixmap textPixmap(captionWidth+2*thickness, captionHeight+2*thickness);
    if(Handler()->titleShadow()) {
        textPixmap.fill(QColor(0,0,0) );

        painter.begin(&textPixmap);
        painter.setFont(s_titleFont);
        painter.setPen(Qt::white);
        painter.drawText(textPixmap.rect(), AlignCenter, c );

        painter.end();
    }

    QPixmap *captionPixmap = new QPixmap(captionWidth+2*thickness, th);

    painter.begin(captionPixmap);
    painter.drawTiledPixmap(captionPixmap->rect(),
                            Handler()->pixmap(TitleBarTile, active, isToolWindow()) );
    if(Handler()->titleShadow())
    {
        QColor shadowColor;
        if (qGray(Handler()->getColor(TitleFont,active).rgb()) < 100)
            shadowColor = QColor(255, 255, 255);
        else
            shadowColor = QColor(0,0,0);
        paintShadow(&painter, textPixmap, 0,1, thickness, shadowColor);
    }
    painter.setFont(s_titleFont);
    painter.setPen(Handler()->getColor(TitleFont,active) );
    painter.drawText(captionPixmap->rect(), AlignCenter, c );
    painter.end();

    m_captionPixmaps[active] = captionPixmap;
    return *captionPixmap;
}

void PlastikClient::clearCaptionPixmaps()
{
    for (int i = 0; i < 2; ++i) {
        delete m_captionPixmaps[i];
        m_captionPixmaps[i] = 0;
    }

    oldCaption = caption();
}

} // KWinPlastik
