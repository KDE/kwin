//////////////////////////////////////////////////////////////////////////////
// oxygenclient.cpp
// -------------------
// Oxygen window decoration for KDE
// -------------------
// Copyright (c) 2003, 2004 David Johnson
// Copyright (c) 2006, 2007 Casper Boemann <cbr@boemann.dk>
// Copyright (c) 2006, 2007 Riccardo Iaconelli <ruphy@fsfe.org>
//
// Please see the header file for copyright and license information.
//////////////////////////////////////////////////////////////////////////////
// #ifndef OXYGENCLIENT_H
// #define OXYGENCLIENT_H

#include <kconfig.h>
#include <kglobal.h>
#include <klocale.h>
#include <kdebug.h>

#include <qbitmap.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qtooltip.h>
//Added by qt3to4:
#include <QBoxLayout>
#include <QGridLayout>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QEvent>
#include <QShowEvent>
#include <QPaintEvent>
#include <QPainterPath>
#include <QTimer>
#include <QCache>

#include "lib/helper.h"

#include "oxygenclient.h"
#include "oxygenclient.moc"
#include "oxygenbutton.h"
#include "oxygen.h"

#include "definitions.cpp"

namespace Oxygen
{

// global constants

// static const int BUTTONSIZE      = 18;
// static const int DECOSIZE        = 8;
// static const int TITLESIZE       = 18;
// static const int TFRAMESIZE       = 8;
// static const int BFRAMESIZE       = 7;
// static const int LFRAMESIZE       = 8;
// static const int RFRAMESIZE       = 7;BUTTONSIZE
// static const int FRAMEBUTTONSPACE       = 67;

K_GLOBAL_STATIC_WITH_ARGS(OxygenHelper, globalHelper, ("OxygenDeco"))

void renderDot(QPainter *p, const QPointF &point, qreal diameter)
{
    p->drawEllipse(QRectF(point.x()-diameter/2, point.y()-diameter/2, diameter, diameter));
}


OxygenClient::OxygenClient(KDecorationBridge *b, KDecorationFactory *f)
    : KCommonDecoration(b, f) { ; }

OxygenClient::~OxygenClient()
{
}
QString OxygenClient::visibleName() const
{
    return i18n("Oxygen");
}

QString OxygenClient::defaultButtonsLeft() const
{
    return "M";
}

QString OxygenClient::defaultButtonsRight() const
{
    return "HIAX";
}

void OxygenClient::init()
{
    KCommonDecoration::init();

    widget()->setAutoFillBackground(false);
    widget()->setAttribute(Qt::WA_OpaquePaintEvent);
    widget()->setAttribute( Qt::WA_PaintOnScreen, false);
}

bool OxygenClient::decorationBehaviour(DecorationBehaviour behaviour) const
{
    switch (behaviour) {
        case DB_MenuClose:
            return true;//Handler()->menuClose();

        case DB_WindowMask:
            return false;

        default:
            return KCommonDecoration::decorationBehaviour(behaviour);
    }
}

int OxygenClient::layoutMetric(LayoutMetric lm, bool respectWindowState, const KCommonDecorationButton *btn) const
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
                return BFRAMESIZE;
            }
        }

        case LM_TitleEdgeTop:
        {
            if (respectWindowState && maximized) {
                return 0;
            } else {
                return TFRAMESIZE;
            }
        }

        case LM_TitleEdgeBottom:
        {
            return 0;
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
                return OXYGEN_BUTTONSIZE;
            } else {
                return OXYGEN_BUTTONSIZE;
            }
        }

        case LM_ButtonSpacing:
            return 1;

        case LM_ButtonMarginTop:
            return 0;

        case LM_ExplicitButtonSpacer:
            return 3;

        default:
            return KCommonDecoration::layoutMetric(lm, respectWindowState, btn);
    }
}


KCommonDecorationButton *OxygenClient::createButton(::ButtonType type)
{
    switch (type) {
        case MenuButton:
            return new OxygenButton(this, i18n("Menu"), ButtonMenu);

        case HelpButton:
            return new OxygenButton(this, i18n("Help"), ButtonHelp);

        case MinButton:
            return new OxygenButton(this, i18n("Minimize"), ButtonMin);

        case MaxButton:
            return new OxygenButton(this, i18n("Minimize"), ButtonMax);

        case CloseButton:
            return new OxygenButton(this, i18n("Minimize"), ButtonClose);

        default:
            return 0;
    }
}


void OxygenClient::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e)
    if (!OxygenFactory::initialized()) return;

    doShape();

    QPalette palette = widget()->palette();
    QPainter painter(widget());

    // ### - This feels like a kwin bug; the palette we get back always seems
    // to be (incorrectly) using the Inactive group, which is wrong; active
    // windows should have currentColorGroup() == Active. So, hack around it...
    // I don't think a window can be disabled?
    if (isActive())
        palette.setCurrentColorGroup(QPalette::Active);
    else
        palette.setCurrentColorGroup(QPalette::Inactive);

    int x,y,w,h;
    QRect frame = widget()->frameGeometry();
    QColor color = palette.window();

    const int titleHeight = layoutMetric(LM_TitleHeight);
    const int titleTop = layoutMetric(LM_TitleEdgeTop) + frame.top();
    const int titleEdgeLeft = layoutMetric(LM_TitleEdgeLeft);
    const int marginLeft = layoutMetric(LM_TitleBorderLeft);
    const int marginRight = layoutMetric(LM_TitleBorderRight);

    const int titleLeft = frame.left() + titleEdgeLeft + buttonsLeftWidth() + marginLeft;
    const int titleWidth = frame.width() -
            titleEdgeLeft - layoutMetric(LM_TitleEdgeRight) -
            buttonsLeftWidth() - buttonsRightWidth() -
            marginLeft - marginRight;


    int splitY = qMin(300, 3*frame.height()/4);

    QPixmap tile = globalHelper->verticalGradient(color, splitY);
    painter.drawTiledPixmap(QRect(0, 0, frame.width(), titleHeight + TFRAMESIZE), tile);

    painter.drawTiledPixmap(QRect(0, 0, LFRAMESIZE, splitY), tile);
    painter.fillRect(0, splitY, LFRAMESIZE, frame.height() - splitY, globalHelper->backgroundBottomColor(color));

    painter.drawTiledPixmap(QRect(frame.width()-RFRAMESIZE, 0,
                                                        RFRAMESIZE, splitY), tile,
                                                        QPoint(frame.width()-RFRAMESIZE, 0));
    painter.fillRect(frame.width()-RFRAMESIZE, splitY, RFRAMESIZE, frame.height() - splitY, globalHelper->backgroundBottomColor(color));

    painter.fillRect(0, frame.height() - BFRAMESIZE, frame.width(), BFRAMESIZE, globalHelper->backgroundBottomColor(color));

    int radialW = qMin(600, frame.width());
    tile = globalHelper->radialGradient(color, radialW);
    QRect radialRect = QRect((frame.width() - radialW) / 2, 0, radialW, 64);
    painter.drawPixmap(radialRect, tile);

    //painter.setRenderHint(QPainter::Antialiasing,true);

    // draw title text
    painter.setFont(options()->font(isActive(), false));
    painter.setPen(palette.windowText());
    painter.drawText(titleLeft, titleTop, titleWidth, titleHeight,
              OxygenFactory::titleAlign() | Qt::AlignVCenter, caption());

    painter.setRenderHint(QPainter::Antialiasing);

    // shadows of the frame
    frame = widget()->rect();
    frame.getRect(&x, &y, &w, &h);

    painter.setBrush(Qt::NoBrush);
    QLinearGradient lg(0, 0, 0, 10);
    QGradientStops stops;
    stops << QGradientStop( 0, QColor(255,255,255, 110) )
           << QGradientStop( 1, QColor(128,128,128, 60) );
    lg.setStops(stops);
    painter.setPen(QPen(QBrush(lg),1));
    painter.drawLine(QPointF(6.3, 0.5), QPointF(w-6.3, 0.5));
    painter.drawArc(QRectF(0.5, 0.5, 9.5, 9.5),90*16, 90*16);
    painter.drawArc(QRectF(w-9.5-0.5, 0.5, 9.5, 9.5), 0, 90*16);

    painter.setPen(QColor(128,128,128, 60));
    painter.drawLine(QPointF(0.5, 6.3), QPointF(0.5, h-6.3));
    painter.drawLine(QPointF(w-0.5, 6.3), QPointF(w-0.5, h-6.3));

    lg = QLinearGradient(0, h-10, 0, h);
    stops.clear();
    stops << QGradientStop( 0, QColor(128,128,128, 60) )
           << QGradientStop( 1, QColor(0,0,0, 50) );
    lg.setStops(stops);
    painter.setPen(QPen(QBrush(lg),1));
    painter.drawArc(QRectF(0.5, h-9.5-0.5, 9.5, 9.5),180*16, 90*16);
    painter.drawArc(QRectF(w-9.5-0.5, h-9.5-0.5, 9.5, 9.5), 270*16, 90*16);
    painter.drawLine(QPointF(6.3, h-0.5), QPointF(w-6.3, h-0.5));

    qreal cenY = frame.height() / 2 + 0.5;
    qreal posX = frame.width() - 2.5;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 66));
    renderDot(&painter, QPointF(posX, cenY - 3), 1.8);
    renderDot(&painter, QPointF(posX, cenY), 1.8);
    renderDot(&painter, QPointF(posX, cenY + 3), 1.8);

    painter.translate(frame.width()-9, frame.height()-9);
    renderDot(&painter, QPointF(2.5, 6.5), 1.8);
    renderDot(&painter, QPointF(5.5, 5.5), 1.8);
    renderDot(&painter, QPointF(6.5, 2.5), 1.8);
}

void OxygenClient::doShape()
{
  int r=widget()->width();
  int b=widget()->height();
QRegion mask(0,0,r,b);
    // Remove top-left corner.
    mask -= QRegion(0, 0, 3, 1);
    mask -= QRegion(0, 1, 2, 1);
    mask -= QRegion(0, 2, 1, 1);

    // Remove top-right corner.
    mask -= QRegion(r - 3, 0, 3, 1);
    mask -= QRegion(r - 2, 1, 2, 1);
    mask -= QRegion(r - 1, 2, 1, 1);

    // Remove bottom-left corner.
    mask -= QRegion(0, b-1-0, 3, b-1-1);
    mask -= QRegion(0, b-1-1, 2, b-1-1);
    mask -= QRegion(0, b-1-2, 1, b-1-1);

    // Remove bottom-right corner.
    mask -= QRegion(r - 3, b-1-0, 3, b-1-1);
    mask -= QRegion(r - 2, b-1-1, 2, b-1-1);
    mask -= QRegion(r - 1, b-1-2, 1, b-1-1);

    setMask(mask);
}

} //namespace Oxygen

//#include "oxygenclient.moc"

// #endif
