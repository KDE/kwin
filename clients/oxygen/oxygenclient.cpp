//////////////////////////////////////////////////////////////////////////////
// oxygenclient.cpp
// -------------------
// Oxygen window decoration for KDE
// -------------------
// Copyright (c) 2006, 2007 Casper Boemann <cbr@boemann.dk>
// Copyright (c) 2006, 2007 Riccardo Iaconelli <riccardo@kde.org>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//////////////////////////////////////////////////////////////////////////////
// #ifndef OXYGENCLIENT_H
// #define OXYGENCLIENT_H

#include "oxygenclient.h"

#include <KConfig>
#include <KGlobal>
#include <KLocale>
#include <KDebug>
#include <KColorUtils>

#include <QBitmap>
#include <QLabel>
#include <QLayout>
#include <QPainter>
#include <QToolTip>
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

#include "math.h"

#include "oxygenclient.moc"
#include "oxygenbutton.h"
#include "oxygen.h"

namespace Oxygen
{

K_GLOBAL_STATIC_WITH_ARGS(OxygenHelper, globalHelper, ("OxygenDeco"))

void renderDot(QPainter *p, const QPointF &point, qreal diameter)
{
    p->drawEllipse(QRectF(point.x()-diameter/2, point.y()-diameter/2, diameter, diameter));
}


OxygenClient::OxygenClient(KDecorationBridge *b, KDecorationFactory *f)
    : KCommonDecoration(b, f)
    , colorCacheInvalid_(true)
    , helper_(*globalHelper)
{
}

OxygenClient::~OxygenClient()
{
}
QString OxygenClient::visibleName() const
{
    return i18n("Oxygen");
}

void OxygenClient::init()
{
    KCommonDecoration::init();

    widget()->setAutoFillBackground(false);
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

        default:
            return KCommonDecoration::layoutMetric(lm, respectWindowState, btn);
    }
}


KCommonDecorationButton *OxygenClient::createButton(::ButtonType type)
{
    switch (type) {
        case MenuButton:
            return new OxygenButton(*this, i18n("Menu"), ButtonMenu);

        case HelpButton:
            return new OxygenButton(*this, i18n("Help"), ButtonHelp);

        case MinButton:
            return new OxygenButton(*this, i18n("Minimize"), ButtonMin);

        case MaxButton:
            return new OxygenButton(*this, i18n("Maximize"), ButtonMax);

        case CloseButton:
            return new OxygenButton(*this, i18n("Close"), ButtonClose);

        case AboveButton:
            return new OxygenButton(*this, i18n("Keep Above Others"), ButtonAbove);

        case BelowButton:
            return new OxygenButton(*this, i18n("Keep Below Others"), ButtonBelow);

        case OnAllDesktopsButton:
            return new OxygenButton(*this, i18n("On All Desktops"), ButtonSticky);

        default:
            return 0;
    }
}


// c0 - background
// c1 - foreground
// t - target contrast ratio
QColor reduceContrast(const QColor &c0, const QColor &c1, double t)
{
    double s = KColorUtils::contrastRatio(c0, c1);
    if (s < t)
        return c1;

    double l = 0.0, h = 1.0;
    double x = s, a;
    QColor r = c1;
    for (int maxiter = 16; maxiter; --maxiter) {
        a = 0.5 * (l + h);
        r = KColorUtils::mix(c0, c1, a);
        x = KColorUtils::contrastRatio(c0, r);
        if (fabs(x - t) < 0.01)
            break;
        if (x > t)
            h = a;
        else
            l = a;
    }
    return r;
}


QColor OxygenClient::titlebarTextColor(const QPalette &palette)
{
    if (isActive())
        return palette.color(QPalette::Active, QPalette::WindowText);
    else {
        if(colorCacheInvalid_) {
            QColor ab = palette.color(QPalette::Active, QPalette::Window);
            QColor af = palette.color(QPalette::Active, QPalette::WindowText);
            QColor nb = palette.color(QPalette::Inactive, QPalette::Window);
            QColor nf = palette.color(QPalette::Inactive, QPalette::WindowText);

            colorCacheInvalid_ = false;
            cachedTitlebarTextColor_ = reduceContrast(nb, nf, qMax(qreal(2.5), KColorUtils::contrastRatio(ab, KColorUtils::mix(ab, af, 0.4))));
        }
        return cachedTitlebarTextColor_;
    }
}


void OxygenClient::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e)
    if (!OxygenFactory::initialized()) return;

    updateButtons();

    QPalette palette = widget()->palette();
    QPainter painter(widget());

    // Set palette to the right group.
    // TODO - fix KWin to do this for us :-).
    if (isActive())
        palette.setCurrentColorGroup(QPalette::Active);
    else
        palette.setCurrentColorGroup(QPalette::Inactive);

    int x,y,w,h;
    QRect frame = widget()->frameGeometry();
    QColor color = palette.window().color();
    QColor light = helper_.calcLightColor( color );
    QColor dark = helper_.calcDarkColor( color );

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

    // draw window background
    helper_.renderWindowBackground(&painter, frame, this->widget(), palette, 0);
    
    // draw title text
    painter.setFont(options()->font(isActive(), false));
    painter.setPen(titlebarTextColor(palette));
    painter.drawText(titleLeft, titleTop-1, titleWidth, titleHeight,  // -1 is to go into top resizearea
              OxygenFactory::titleAlign() | Qt::AlignVCenter, caption());

    painter.setRenderHint(QPainter::Antialiasing);

    // Draw dividing line
    frame = widget()->rect();
    frame.adjust(1,1,-1,-1);
    frame.getRect(&x, &y, &w, &h);

    dark.setAlpha(120);

    if(isActive()) {
        QLinearGradient lg(x,0,x+w,0);
        lg.setColorAt(0.5, dark);
        dark.setAlpha(0);
        lg.setColorAt(0.0, dark);
        lg.setColorAt(1.0, dark);
        painter.setPen(QPen(lg,1));

        painter.drawLine(QPointF(x, titleTop+titleHeight-1.5),
                                QPointF(x+w, titleTop+titleHeight-1.5));

        lg = QLinearGradient(x,0,x+w,0);
        lg.setColorAt(0.5, light);
        light.setAlpha(0);
        lg.setColorAt(0.0, light);
        lg.setColorAt(1.0, light);
        painter.setPen(QPen(lg,1));

        painter.drawLine(QPointF(x, titleTop+titleHeight-0.5),
                               QPointF(x+w, titleTop+titleHeight-0.5));
    }

    // draw stripes as indicator for active windows
    if (isActive() && OxygenFactory::showStripes()) {
        Qt::Alignment align = OxygenFactory::titleAlign();
        if (widget()->layoutDirection() == Qt::RightToLeft)
        {
            if (align == Qt::AlignLeft)
                align = Qt::AlignRight;
            else if (align == Qt::AlignRight)
                align = Qt::AlignLeft;
        }

        if (align & Qt::AlignLeft) {
            int left = titleLeft + QFontMetrics(options()->font(isActive(), false)).width(caption());
            int right = titleLeft + titleWidth;
            drawScratch(&painter, palette, left, right, titleTop+6);
        }
        if (align & Qt::AlignRight) {
            int left = titleLeft;
            int right = titleLeft + titleWidth - QFontMetrics(options()->font(isActive(), false)).width(caption());
            drawScratch(&painter, palette, right, left, titleTop+6);
        }
        if (align & Qt::AlignHCenter) {
            int textWidth = QFontMetrics(options()->font(isActive(), false)).width(caption());
            int left = titleLeft;
            int centerLeft = titleLeft + titleWidth/2 - textWidth/2;
            int centerRight = titleLeft + titleWidth/2 + textWidth/2;
            int right = titleLeft + titleWidth;
            drawScratch(&painter, palette, centerLeft, left, titleTop+6);
            drawScratch(&painter, palette, centerRight, right, titleTop+6);
        }
    }

    // Draw shadows of the frame
    bool maximized = maximizeMode()==MaximizeFull && !options()->moveResizeMaximizedWindows();

    if(maximized)
        return;

    helper_.drawFloatFrame(&painter, widget()->rect(), color);

    if(!isResizable())
        return;

    // Draw the 3-dots resize handles
    qreal cenY = frame.height() / 2 + x + 0.5;
    qreal posX = frame.width() + y - 2.5;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 66));
    renderDot(&painter, QPointF(posX, cenY - 3), 1.8);
    renderDot(&painter, QPointF(posX, cenY), 1.8);
    renderDot(&painter, QPointF(posX, cenY + 3), 1.8);

    painter.translate(x + frame.width()-9, y + frame.height()-9);
    renderDot(&painter, QPointF(2.5, 6.5), 1.8);
    renderDot(&painter, QPointF(5.5, 5.5), 1.8);
    renderDot(&painter, QPointF(6.5, 2.5), 1.8);
}

void OxygenClient::drawScratch(QPainter *p, QPalette &palette, const int start, const int end, const int topMargin)
{
    QLinearGradient scratchlg(QPoint(start,0), QPoint(end,0));
    scratchlg.setColorAt(0.0, Qt::transparent);
    scratchlg.setColorAt(0.05, KDecoration::options()->color(ColorTitleBar));
    scratchlg.setColorAt(1.0, Qt::transparent);
    QPen pen1(scratchlg, 0.5);

    QLinearGradient scratchlg2(QPoint(start,0), QPoint(end,0));
    scratchlg2.setColorAt(0.0, Qt::transparent);
    scratchlg2.setColorAt(0.05, helper_.calcLightColor(palette.color(QPalette::Window)));
    scratchlg2.setColorAt(1.0, Qt::transparent);
    QPen pen2(scratchlg2, 0.5);

    bool antialiasing = p->testRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::Antialiasing, false);
    for (int i = 0; i < 3; ++i)
    {
        p->setPen(pen1);
        p->drawLine(QPointF(start, topMargin+4*i), QPointF(end, topMargin+4*i));
        p->setPen(pen2);
        p->drawLine(QPointF(start, topMargin+4*i+1), QPointF(end, topMargin+4*i+1));
    }
    p->setRenderHint(QPainter::Antialiasing, antialiasing);
}


void OxygenClient::updateWindowShape()
{
    bool maximized = maximizeMode()==MaximizeFull && !options()->moveResizeMaximizedWindows();
    int w=widget()->width();
    int h=widget()->height();

    if(maximized) {
        QRegion mask(0,0,w,h);
        setMask(mask);
        return;
    }

    QRegion mask(4, 0, w-8, h);
    mask += QRegion(0, 4, w, h-8);
    mask += QRegion(2, 1, w-4, h-2);
    mask += QRegion(1, 2, w-2, h-4);

    setMask(mask);
}

} //namespace Oxygen

//#include "oxygenclient.moc"

// #endif
