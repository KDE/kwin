//////////////////////////////////////////////////////////////////////////////
// oxygenbutton.cpp
// -------------------
// Oxygen window decoration for KDE. Buttons.
// -------------------
// Copyright (c) 2006, 2007 Riccardo Iaconelli <ruphy@fsfe.org>
// Copyright (c) 2006, 2007 Casper Boemann <cbr@boemann.dk>
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
#include <math.h>
#include <QPainterPath>
#include <QPainter>
#include <QBitmap>

#include <kdecoration.h>
#include <kglobal.h>

#include "oxygenclient.h"
#include "oxygenbutton.h"
#include "oxygen.h"
#include "definitions.cpp"

namespace Oxygen
{
// class OxygenClient;
/*
extern int BUTTONSIZE;
extern int DECOSIZE;*/

// static const int DECOSIZE        = 8;
//////////////////////////////////////////////////////////////////////////////
// OxygenButton Class                                                      //
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// OxygenButton()
// ---------------
// Constructor

OxygenButton::OxygenButton(OxygenClient &parent,
                             const QString& tip, ButtonType type)
    : KCommonDecorationButton((::ButtonType)type, &parent)
    , client_(parent)
    , helper_(parent.helper_)
    , type_(type)
    , lastmouse_(0)
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setFixedSize(OXYGEN_BUTTONSIZE, OXYGEN_BUTTONSIZE);
    setCursor(Qt::ArrowCursor);
    setToolTip(tip);
}

OxygenButton::~OxygenButton()
{
}

//////////////////////////////////////////////////////////////////////////////
// sizeHint()
// ----------
// Return size hint

QSize OxygenButton::sizeHint() const
{
    return QSize(OXYGEN_BUTTONSIZE, OXYGEN_BUTTONSIZE);
}

//////////////////////////////////////////////////////////////////////////////
// enterEvent()
// ------------
// Mouse has entered the button

void OxygenButton::enterEvent(QEvent *e)
{
    // if we wanted to do mouseovers, we would keep track of it here
    if (status_ != Oxygen::Pressed) {
        status_ = Oxygen::Hovered;
    }
    QAbstractButton::enterEvent(e);
}

//////////////////////////////////////////////////////////////////////////////
// leaveEvent()
// ------------
// Mouse has left the button

void OxygenButton::leaveEvent(QEvent *e)
{
    // if we wanted to do mouseovers, we would keep track of it here
    status_ = Oxygen::Normal;
    QAbstractButton::leaveEvent(e);
}

//////////////////////////////////////////////////////////////////////////////
// pressSlot()
// ------------
// Mouse has pressed the button

void OxygenButton::pressSlot()
{
    status_ = Oxygen::Pressed;
    update();
}
//////////////////////////////////////////////////////////////////////////////
// drawButton()
// ------------
// Draw the button

void OxygenButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    QPalette pal = palette(); // de-const-ify

    if (type_ == ButtonMenu) {
        // we paint the mini icon (which is 16 pixels high)
        int dx = (width() - 16) / 2;
        int dy = (height() - 16) / 2;
        painter.drawPixmap(dx, dy, client_.icon().pixmap(16));
        return;
    }

    // Set palette to the right group. Lubos disagrees with this being a kwin
    // bug, but anyway, we need the palette group to match the current window.
    // Since kwin doesn't set it correctly, we have to do it ourselves.
    if (client_.isActive())
        pal.setCurrentColorGroup(QPalette::Active);
    else
        pal.setCurrentColorGroup(QPalette::Inactive);

    QColor bg = helper_.backgroundTopColor(pal.window());
    painter.drawPixmap(0, 0, helper_.windecoButton(pal.button()));

    painter.setRenderHints(QPainter::Antialiasing);
    painter.setBrush(Qt::NoBrush);
    QLinearGradient lg = helper_.decoGradient(QRect(4,4,13,13), pal.color(QPalette::ButtonText));
    painter.setPen(QPen(lg, 2.5));
    switch(type_)
    {
        case ButtonSticky:
            painter.drawPoint(QPointF(10.5,10.5));
            break;
        case ButtonHelp:
            painter.translate(1.5, 1.5);
            painter.drawArc(7,5,4,4,135*16, -180*16);
            painter.drawArc(9,8,4,4,135*16,45*16);
            painter.drawPoint(9,12);
            break;
        case ButtonMin:
            painter.drawLine(QPointF( 7.5, 9.5), QPointF(10.5,12.5));
            painter.drawLine(QPointF(10.5,12.5), QPointF(13.5, 9.5));
            break;
        case ButtonMax:
            switch(client_.maximizeMode())
            {
                case OxygenClient::MaximizeRestore:
                case OxygenClient::MaximizeVertical:
                case OxygenClient::MaximizeHorizontal:
                    painter.drawLine(QPointF( 7.5,11.5), QPointF(10.5, 8.5));
                    painter.drawLine(QPointF(10.5, 8.5), QPointF(13.5,11.5));
                    break;
                case OxygenClient::MaximizeFull:
                {
                    painter.translate(1.5, 1.5);
                    painter.setBrush(lg);
                    QPoint points[4] = {QPoint(9, 6), QPoint(12, 9), QPoint(9, 12), QPoint(6, 9)};
                    painter.drawPolygon(points, 4);
                    break;
                }
            }
            break;
        case ButtonClose:
            painter.drawLine(QPointF( 7.5,7.5), QPointF(13.5,13.5));
            painter.drawLine(QPointF(13.5,7.5), QPointF( 7.5,13.5));
            break;
        default:
            break;
    }
}








} //namespace Oxygen
