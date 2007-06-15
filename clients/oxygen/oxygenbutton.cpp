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

// static const int BUTTONSIZE      = 18;
// static const int DECOSIZE        = 8;
//////////////////////////////////////////////////////////////////////////////
// OxygenButton Class                                                      //
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// OxygenButton()
// ---------------
// Constructor

OxygenButton::OxygenButton(OxygenClient *parent,
                             const QString& tip, ButtonType type,
                             const unsigned char *bitmap)
    : QAbstractButton(parent->widget()), client_(parent), type_(type),
      deco_(0), lastmouse_(0)
{
    //setBackgroundMode(Qt::NoBackground); PORT to qt4
    setFixedSize(BUTTONSIZE, BUTTONSIZE);
    setCursor(Qt::ArrowCursor);
    if (bitmap) setBitmap(bitmap);
    setToolTip(tip);
    connect(this, SIGNAL(pressed()), this, SLOT(pressSlot()));
}

OxygenButton::~OxygenButton()
{
    if (deco_) delete deco_;
}

//////////////////////////////////////////////////////////////////////////////
// setBitmap()
// -----------
// Set the button decoration

void OxygenButton::setBitmap(const unsigned char *bitmap)
{
    if (!bitmap) return; // no bitmap, probably the menu button

    if (deco_) delete deco_;
    deco_ = new QBitmap(DECOSIZE, DECOSIZE);
    //PORT to qt4 set actual pixels   , bitmap, true);
    deco_->setMask(*deco_);
    update();
}

//////////////////////////////////////////////////////////////////////////////
// sizeHint()
// ----------
// Return size hint

QSize OxygenButton::sizeHint() const
{
    return QSize(BUTTONSIZE, BUTTONSIZE);
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
    kDebug() << "Pressed " << endl;
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

    if (type_ == ButtonMenu) {
        // we paint the mini icon (which is 16 pixels high)
        int dx = (width() - 16) / 2;
        int dy = (height() - 16) / 2;
        painter.drawPixmap(dx, dy, client_->icon().pixmap(16));
        return;
    }

    QRadialGradient grad1(QPointF(5.0, 5.0),5.0, QPointF(5.0, 7.5));
    grad1.setColorAt(0.0, QColor(0,0,0,255));
    grad1.setColorAt(1.0, QColor(0,0,0,0));
    QRadialGradient grad2(QPointF(5.0, 5.0),5.0, QPointF(5.0, 8.0));
    grad2.setColorAt(0.0, QColor(240,240,240));
    grad2.setColorAt(1.0, QColor(220,220,220));
    QRadialGradient grad3(QPointF(5.0, 3.75), 3.5,QPointF(5.0, 2.5));
    grad3.setColorAt(0, QColor(255,255,255,211));
    grad3.setColorAt(1, QColor(255,255,255,25));
    QBrush brush1(grad1);
    QBrush brush2(grad2);
    QBrush brush3(grad3);

    painter.scale(width()/10.0, height()/10.0);
    painter.setRenderHint(QPainter::Antialiasing,true);

    QPainterPath path1;
    path1.addEllipse(0.0, 0.0, 10.0, 10.0);
    painter.fillPath(path1, brush1);

    QPainterPath path2;
    path2.addEllipse(0.5, 0.5, 9.0, 9.0);
    painter.fillPath(path2, brush2);
    
    
    QLinearGradient grad8;
    QLinearGradient grad7;
    
    if (type_ == Oxygen::ButtonClose){
        //Draw the red 'x'
        painter.save();
        grad7.setStart(0,0);
        grad7.setFinalStop(3, 7);
        if (status_ == Oxygen::Hovered) {
            grad7.setColorAt(0, Qt::red);
            grad7.setColorAt(1, Qt::black);
        } else if (status_ == Oxygen::Normal) {
            grad7.setColorAt(0, QColor(91, 0, 0, 144));
            grad7.setColorAt(1, QColor(91, 0, 0, 144));
        }
        painter.setBrush(grad7);
        painter.translate(3.3, 2.5);
        painter.rotate(45);
        painter.setPen(Qt::NoPen);
        painter.drawRoundRect(0, 0, 6, 1, 0, 0);
        painter.translate(3.5, -2.5);
        painter.rotate(90);
        painter.drawRoundRect(0, 0, 6, 1, 0, 0);
        painter.restore();
    } else if (type_ == Oxygen::ButtonMax){
        //Draw the green '+'
        painter.save();
        grad7.setStart(0,0);
        grad7.setFinalStop(3, 7);
        grad8.setStart(0,0);
        grad8.setFinalStop(7, 3);
        if (status_ == Oxygen::Hovered) {
            grad7.setColorAt(0, QColor(0, 80, 0, 50));
            grad7.setColorAt(1, QColor(0, 80, 0, 50));
            grad8.setColorAt(0, QColor(0, 80, 0, 50));
            grad8.setColorAt(1, QColor(0, 80, 0, 50));
        } else if (status_ == Oxygen::Normal) {
            grad7.setColorAt(0, QColor(0, 0, 0, 15));
            grad7.setColorAt(1, QColor(0, 0, 0, 15));
            grad8.setColorAt(0, QColor(0, 0, 0, 15));
            grad8.setColorAt(1, QColor(0, 0, 0, 15));
        } else { //Pressed
            grad7.setColorAt(0, Qt::green);
            grad7.setColorAt(1, Qt::green);
            grad8.setColorAt(0, Qt::green);
            grad8.setColorAt(1, Qt::green);
        }
        painter.setBrush(grad8);
        painter.translate(2.0, 4.5);
        painter.setPen(Qt::NoPen);
        painter.drawRoundRect(0, 0, 6, 1, 0, 0);
        painter.translate(3.5, -2.5);
        painter.setBrush(grad7);
        painter.rotate(90);
        painter.drawRoundRect(0, 0, 6, 1, 0, 0);
        painter.restore();
    } else if (type_ == Oxygen::ButtonMin){
        //Draw the yellow '-'
        painter.save();
//         QLinearGradient grad7;
//         grad7.setStart(0,0);
//         grad7.setFinalStop(3, 7);
        grad8.setStart(0,0);
        grad8.setFinalStop(7, 3);
        if (status_ == Oxygen::Hovered) {
//         grad7.setColorAt(0, QColor(0, 91, 0, 144));
//         grad7.setColorAt(1, QColor(0, 91, 0, 144));
            grad8.setColorAt(0, QColor(155, 121, 0, 155));
            grad8.setColorAt(1, QColor(155, 121, 0, 155));
        } else if (status_ == Oxygen::Normal) {
//             grad7.setColorAt(0, QColor(0, 0, 0, 144));
//             grad7.setColorAt(1, QColor(0, 0, 0, 144));
            grad8.setColorAt(0, QColor(0, 0, 0, 30));
            grad8.setColorAt(1, QColor(0, 0, 0, 30));
        } else if (status_ == Oxygen::Pressed){ //Pressed
//             grad7.setColorAt(0, Qt::yellow);
//             grad7.setColorAt(1, Qt::yellow);
            grad8.setColorAt(0, Qt::yellow);
            grad8.setColorAt(1, Qt::yellow);
        }
        painter.setBrush(grad8);
        painter.translate(2.0, 4.5);
        painter.setPen(Qt::NoPen);
        painter.drawRoundRect(0, 0, 6, 1, 0, 0);
//         painter.translate(3.5, -2.5);
//         painter.setBrush(grad7);
//         painter.rotate(90);
//         painter.drawRoundRect(0, 0, 6, 1, 0, 0);
        painter.restore();
    } else if (type_ == Oxygen::ButtonHelp){
        //Draw the red 'x'
        painter.save();
//         QLinearGradient grad7;
//         grad7.setStart(0,0);
//         grad7.setFinalStop(3, 7);
        grad8.setStart(0,0);
        grad8.setFinalStop(7, 3);
        if (status_ == Oxygen::Hovered) {
//         grad7.setColorAt(0, QColor(0, 91, 0, 144));
//         grad7.setColorAt(1, QColor(0, 91, 0, 144));
            grad8.setColorAt(0, QColor(155, 121, 0, 155));
            grad8.setColorAt(1, QColor(155, 121, 0, 155));
        } else if (status_ == Oxygen::Normal) {
//             grad7.setColorAt(0, QColor(0, 0, 0, 144));
//             grad7.setColorAt(1, QColor(0, 0, 0, 144));
            grad8.setColorAt(0, QColor(0, 0, 0, 30));
            grad8.setColorAt(1, QColor(0, 0, 0, 30));
        } else if (status_ == Oxygen::Pressed){ //Pressed
//             grad7.setColorAt(0, Qt::yellow);
//             grad7.setColorAt(1, Qt::yellow);
            grad8.setColorAt(0, Qt::yellow);
            grad8.setColorAt(1, Qt::yellow);
        }
        //This code has been picked up by a drawing made with inkscape
        QPolygonF questionMark;
        questionMark << QPointF(37.918475,94.451027) << QPointF(36.026474,85.486156)
                     << QPointF(36.772553,75.80096) << QPointF(41.423128,66.688939)
                     << QPointF(49.497475,61.402536) << QPointF(58.596523,56.663413)
                     << QPointF(68.690373,56.194458) << QPointF(78.505617,59.46141) 
                     << QPointF(86.699378,68.158973) << QPointF(90.881465,78.404561) 
                     << QPointF(90.345804,89.078756) << QPointF(88.320755,95.66811) 
                     << QPointF(81.408421,102.0783) << QPointF(69.198471,108.7171) 
                     << QPointF(64.91,116.86) << QPointF(63.067541,120.47646) 
                     << QPointF(62.467671,131.84155) << QPointF(62.6367,146.0);

        QMatrix inkscapeMatrix(0.09245,0,0,0.0916,-3.2868,-5.1);

        painter.translate(2.0, 4.5);
        painter.setWorldMatrix(inkscapeMatrix);
//         painter.setBrush(grad8);
        QPen oxyPen;  // creates a default pen
        oxyPen.setStyle(Qt::DashDotLine);
        oxyPen.setWidth(5);
        oxyPen.setBrush(grad8);
        oxyPen.setCapStyle(Qt::RoundCap);
        oxyPen.setJoinStyle(Qt::RoundJoin);

        painter.setPen(oxyPen);
//         painter.setPen(grad8);
        painter.drawPolygon(questionMark);
//         painter.drawRoundRect(0, 0, 6, 1, 0, 0);
//         painter.translate(3.5, -2.5);
//         painter.setBrush(grad7);
//         painter.rotate(90);
//         painter.drawRoundRect(0, 0, 6, 1, 0, 0);
        painter.restore();
    }

    //light
    QPainterPath path3;
    path3.addEllipse(1.5, 0.5, 7.0, 6.0);
    painter.fillPath(path3, brush3);

    if (type_ == Oxygen::ButtonMax) {
        painter.save();
        painter.setBrush(grad8);
        painter.translate(2.0, 4.5);
        painter.setPen(Qt::NoPen);
        painter.drawRoundRect(0, 0, 6, 1, 0, 0);
        painter.translate(3.5, -2.5);
        painter.setBrush(grad7);
        painter.rotate(90);
        painter.drawRoundRect(0, 0, 6, 1, 0, 0);
        painter.restore();
    }

//     QPainter painter(this);
// 
//     if (!OxygenFactory::initialized()) return;
// 
//     QPalette palette;
//     int dx, dy;
// 
//     // paint a plain box with border
//     palette = KDecoration::options()->palette(KDecoration::ColorFrame, client_->isActive());
//     painter.fillRect(rect(), palette.button());
//     
//     if (type_ == ButtonMenu) {
//         // we paint the mini icon (which is 16 pixels high)
//         dx = (width() - 16) / 2;
//         dy = (height() - 16) / 2;
// //PORT qt4        painter.drawPixmap(dx, dy, client_->icon().pixmap(QIcon::Small, QIcon::Normal));
//     } else {
// //       painter->fillRect(rect(), palette.button());
//        int x,y,w,h;
//        rect().getRect(&x, &y, &w, &h);
//        painter.setBrush(palette.dark());
//        painter.drawEllipse(x, y, w, h);
// 
//        if (deco_) {
//            // otherwise we paint the deco
//            dx = (width() - DECOSIZE) / 2;
//            dy = (height() - DECOSIZE) / 2;
//            painter.setBrush(palette.dark());
//            painter.drawPixmap(dx, dy, *deco_);
//        }
//     }
}








} //namespace Oxygen
