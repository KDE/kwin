/***************************************************************************
 *   Copyright (C) 2006-2007 by Thomas L�bking                             *
 *   thomas.luebking@web.de                                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <QAction>
#include <QApplication>
#include <QAbstractScrollArea>
#include <QComboBox>
#include <QDesktopWidget>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QStyleOptionButton>
#include <QPainter>
#include <QPainterPath>
#include <QX11Info>
#include "oxygen.h"
#include "oxrender.h"
#include "dynamicbrush.h"

#include <QtDebug>

#include <cmath>

using namespace Oxygen;

extern int bgYoffset_;
extern Pixmap shadowPix;
extern Config config;
extern Dpi dpi;

#include "inlinehelp.cpp"
#include "makros.h"


void OxygenStyle::drawPrimitive ( PrimitiveElement pe, const QStyleOption * option, QPainter * painter, const QWidget * widget) const {
   Q_ASSERT(option);
   Q_ASSERT(painter);
   
   bool sunken = option->state & State_Sunken;
   bool isEnabled = option->state & State_Enabled;
   bool hover = isEnabled && (option->state & State_MouseOver);
   bool hasFocus = option->state & State_HasFocus;
   bool up = false;
   
   
   switch ( pe ) {
   case PE_FrameDefaultButton: // This frame around a default button, e.g. in a dialog.
      // we swap colors instead, frame only on focus!
      break;
   case PE_PanelButtonCommand: // Button used to initiate an action, for example, a QPushButton.
   case PE_PanelButtonBevel: { // Generic panel with a button bevel.
      const int $4 = dpi.$4;
      //bool isOn = option->state & State_On;
      const QStyleOptionButton* opt = qstyleoption_cast<const QStyleOptionButton*>(option);
      //bool isDefault = opt && (opt->features & QStyleOptionButton::DefaultButton);
      
      int step = hoverStep(widget);

      QColor c = btnBgColor(PAL, isEnabled, 0, 0);
      QRect r = RECT;
      
      // shadow
      shadows.button[sunken][step?step:hover*7].render(r, painter);

      r.adjust($4, $4, -$4, -$4);

      // background
      painter->setRenderHint(QPainter::Antialiasing,false);
      fillWithMask(painter, r, c, &masks.button);

      // edge and a nice light gradient
      painter->setRenderHint(QPainter::Antialiasing);
      QLinearGradient lg(r.x(), r.y(), r.x(), r.bottom());
      QGradientStops stops;
      stops << QGradientStop( 0, QColor(255,255,255, (isEnabled&&!sunken ? 220 : 160)) )
           << QGradientStop( 0.1, QColor(255,255,255, (isEnabled&&!sunken ? 100 : 60)) )
           << QGradientStop( 1, QColor(255,255,255, (isEnabled&&!sunken ? 50 : 35)) );
      lg.setStops(stops);
      QLinearGradient lg2(r.x(), r.y(), r.x(), r.bottom());
      stops.clear();
      stops << QGradientStop( 0, QColor(255,255,255, 40) )
           << QGradientStop( 1, QColor(255,255,255, 0) );
      lg2.setStops(stops);
      painter->setPen(QPen(QBrush(lg),1));
      painter->setBrush(lg2);
      QRectF rf = r;
      rf.adjust(0.5, 0.5,-0.5,-0.5);
      painter->drawRoundRect(rf, int(ceil(9*90.0/r.width())), int(ceil(9*90.0/r.height())));

      // hover effect
      QRadialGradient rg = QRadialGradient(r.width()/2.0, 0.35*r.height(), qMax(r.width(),r.height())/2.0 - 5, r.width()/2.0, 0.35*r.height());
      c = btnBgColor(PAL, isEnabled, hover, step);
      c.setAlpha(190);
      QColor c2= c;
      c2.setAlpha(60);
      stops << QGradientStop( 0, c ) << QGradientStop( 1, c2 );
      rg.setStops(stops);
      painter->setPen(Qt::NoPen);
      painter->setBrush(rg);
      r.adjust(1,1,-1,-1);
      painter->drawRoundRect(r, int(ceil(9*90.0/r.width())), int(ceil(9*90.0/r.height())));
      break;
   }
   case PE_PanelButtonTool: { // Panel for a Tool button, used with QToolButton.
      if (sunken || (option->state & State_On)) {
         if (sunken) hover = false;
/*
         fillWithMask(painter, RECT,
                      gradient(COLOR(Window), RECT.height(), Qt::Vertical,
                               hover ? GradButton : GradSunken), &masks.button);
         shadows.lineEdit[1].render(RECT, painter);
*/
         QRect r = RECT;
         painter->setRenderHint(QPainter::Antialiasing);
         QRadialGradient rg(r.width()/2, r.height()/2, r.height()/2, r.width()/2, r.height()/4);
         QGradientStops stops;
         stops << QGradientStop( 0, QColor(0,0,0, 50) )
           << QGradientStop( 1, QColor(0,0,0, 0) );
         rg.setStops(stops);
         painter->fillRect(r, rg);
         rg = QRadialGradient(r.width()/2, r.height()/2, r.height()/2, r.width()/2, 3*r.height()/4);
         stops.clear();
         stops << QGradientStop( 0, QColor(255,255,255, 190) )
           << QGradientStop( 1, QColor(255,255,255, 0) );
         rg.setStops(stops);
         painter->fillRect(r, rg);
      }
/*      else if (hover) {
         fillWithMask(painter, RECT.adjusted(dpi.$2,dpi.$1,-dpi.$2,0),
                      gradient(COLOR(Window), RECT.height(), Qt::Vertical,
                               GradButton), &masks.button);
         shadows.group.render(RECT, painter, Tile::Ring);
      }
*/      break;
   }
   case PE_PanelLineEdit: { // Panel for a QLineEdit.
      // spinboxes and combos allready have a lineedit as global frame
      if (widget && widget->parentWidget() &&
          (widget->parentWidget()->inherits("QAbstractSpinBox") ||
           widget->parentWidget()->inherits("QComboBox")))
         break;
      if (qstyleoption_cast<const QStyleOptionFrame *>(option) &&
          static_cast<const QStyleOptionFrame *>(option)->lineWidth < 1) {
         painter->fillRect(RECT, COLOR(Base)); break;
      }

      if (isEnabled) {
         fillWithMask(painter, RECT, COLOR(Base), &masks.button);
         if (hasFocus) {
            painter->save();
            painter->setPen(QPen(COLOR(Highlight), dpi.$2));
            painter->drawLine(RECT.left()+dpi.$4, RECT.bottom(),
                              RECT.right()-dpi.$3, RECT.bottom());
            painter->restore();
         }
      }
      shadows.lineEdit[isEnabled].render(RECT, painter);
      break;
   }
   case PE_FrameFocusRect: { // Generic focus indicator.
      painter->save();
      painter->setBrush(Qt::NoBrush);
      painter->setPen(COLOR(Highlight));
      painter->drawLine(RECT.bottomLeft(), RECT.bottomRight());
      painter->restore();
      break;
   }
   case PE_IndicatorArrowUp: // Generic Up arrow.
   case PE_IndicatorSpinUp: // Up symbol for a spin widget, for example a QSpinBox.
   case PE_IndicatorSpinPlus: // Increase symbol for a spin widget.
      up = true;
   case PE_IndicatorHeaderArrow: // Arrow used to indicate sorting on a list or table header
   case PE_IndicatorButtonDropDown: // indicator for a drop down button, for example, a tool button that displays a menu.
   case PE_IndicatorArrowDown: // Generic Down arrow.
   case PE_IndicatorSpinDown: // Down symbol for a spin widget.
   case PE_IndicatorSpinMinus: { // Decrease symbol for a spin widget.
      if (const QStyleOptionHeader* hopt =
          qstyleoption_cast<const QStyleOptionHeader*>(option)) {
         if (hopt->sortIndicator == QStyleOptionHeader::None)
            break;
         up = hopt->sortIndicator == QStyleOptionHeader::SortUp;
      }
      SAVE_ANTIALIAS;
      bool hadNoBrush = painter->brush() == Qt::NoBrush;
      painter->setRenderHint(QPainter::Antialiasing);
      if (hadNoBrush)
         painter->setBrush(painter->pen().brush());
      
      int w = RECT.width();
      
      // we want a golden mean cut arrow ;) 1:1.6180339887498948482
      int x[3], y[2];
      if (w <  8*RECT.height()/5) {
         if (w%2) --w;
         x[0] = RECT.x(); x[1] = RECT.right(); x[2] = x[0] + w/2;
         int h = 5*w/8;  if (!(h%2)) --h;
         y[0] = RECT.y() + (RECT.height()-h)/2; y[1] = y[0] + h;
      }
      else {
         w = 8*RECT.height()/5;
         if (w%2) --w;
         x[0] = RECT.x() + (RECT.width()-w)/2; x[1] = x[0] + w; x[2] = x[0] + w/2;
         y[0] = RECT.y(); y[1] = RECT.bottom();
      }
      if (up) {
         //  0.5 is to have sharp horizontal edges
        const QPointF points[3] =  { QPointF(x[0], y[1]+0.5), QPointF(x[1], y[1]+0.5), QPointF(x[2], y[0]) };
         painter->drawPolygon(points, 3);
      }
      else {
        //  0.5 is to have sharp horizontal edges
         const QPointF points[3] =  { QPointF(x[0], y[0]-0.5), QPointF(x[1], y[0]-0.5), QPointF(x[2], y[1]) };
         painter->drawPolygon(points, 3);
      }
      if (hadNoBrush)
         painter->setBrush(Qt::NoBrush);
      RESTORE_ANTIALIAS;
      break;
   }
   case PE_IndicatorArrowRight: // Generic Right arrow.
      up = true;
   case PE_IndicatorArrowLeft: { // Generic Left arrow.
      SAVE_ANTIALIAS;
      bool hadNoBrush = painter->brush() == Qt::NoBrush;
      painter->setRenderHint(QPainter::Antialiasing);
      if (hadNoBrush)
         painter->setBrush(painter->pen().brush());
      int x[2], y[3], h = RECT.height();
      if (h <  8*RECT.width()/5) {
         if (h%2) --h;
         y[0] = RECT.y(); y[1] = RECT.bottom(); y[2] = y[0] + h/2;
         int w = 5*h/8; if (!(w%2)) --w;
         x[0] = RECT.x() + (RECT.width()-w)/2; x[1] = x[0] + w;
      }
      else {
         h = 8*RECT.width()/5;
         if (h%2) --h;
         y[0] = RECT.y() + (RECT.height()-h)/2; y[1] = y[0] + h; y[2] = y[0] + h/2;
         x[0] = RECT.x(); x[1] = RECT.right();
      }
      if (up) { //right
        //  0.5 is to have sharp vertical edge
         const QPointF points[3] =  { QPointF(x[0]+0.5, y[0]), QPointF(x[0]+0.5, y[1]), QPointF(x[1], y[2]) };
         painter->drawPolygon(points, 3);
      }
      else {
         const QPointF points[3] =  { QPointF(x[0], y[2]), QPointF(x[1]-0.5, y[0]), QPointF(x[1]-0.5, y[1]) };
         painter->drawPolygon(points, 3);
      }
      if (hadNoBrush)
         painter->setBrush(Qt::NoBrush);
      RESTORE_ANTIALIAS;
      break;
   }
   case PE_IndicatorCheckBox: { // On/off indicator, for example, a QCheckBox.
      
      drawPrimitive(PE_PanelButtonBevel, option, painter, widget);
      
      if (!(sunken || (option->state & State_Off))) {
         QColor c = btnFgColor(PAL, isEnabled, hover||hasFocus);
         painter->save();
         painter->setRenderHint(QPainter::Antialiasing);
         QRect r = RECT.adjusted(dpi.$6,dpi.$6,-dpi.$6,-dpi.$6);
         const QPixmap &fill = gradient(c, r.height(), Qt::Vertical, config.gradBtn);
         switch (config.checkType) {
         case 0: {
            QPen pen(fill, r.width()/5, Qt::SolidLine, Qt::RoundCap, Qt::BevelJoin);
            painter->setPen(pen);
            painter->drawLine(r.x(),r.bottom(),r.right(),r.y());
            if (option->state & State_On)
               painter->drawLine(r.x(),r.y(),r.right(),r.bottom());
            break;
         }
         default:
         case 1: {
            QPen pen(fill, r.width()/5, Qt::SolidLine, Qt::RoundCap, Qt::BevelJoin);
            painter->setPen(pen);
            const QPoint points[4] = {
            QPoint(r.right(), r.top()),
            QPoint(r.x()+r.width()/3, r.bottom()),
            QPoint(r.x(), r.bottom()-r.height()/3),
            QPoint(r.x()+r.width()/3, r.bottom()-r.height()/5)
            };
            painter->drawPolygon(points, (option->state & State_On)?4:2);
            break;
         }
         case 2:
            if (option->state & State_On)
               painter->fillRect(r, fill);
            else {
               QRect r2 = r; r2.setBottom(r.top()+r.height()/3);
               fillWithMask(painter, r2, fill, &masks.button);
               r2 = r; r2.setTop(r.bottom()-r.height()/3);
               fillWithMask(painter, r2, fill, &masks.button);
            }
         }
         painter->restore();
      }
      break;
   }
   case PE_IndicatorRadioButton: { // Exclusive on/off indicator, for example, a QRadioButton.
      bool isOn = option->state & State_On;
      if (isOn) sunken = false;
      hover = hover && !isOn;
      const int $2 = dpi.$2, $1 = dpi.$1;
      int step = isOn ? 0 : hoverStep(widget);
      QColor c = btnBgColor(PAL, isEnabled, hover||hasFocus, step);
      QPoint xy = RECT.topLeft();
      
      // shadow
      painter->drawPixmap(sunken?xy+QPoint($1,$1):xy, shadows.radio[sunken][hover||hasFocus]);
      
      // glass
      xy += QPoint($2,$1);
      int sz = dpi.ExclusiveIndicator - dpi.$4;
      if (isEnabled)
         fillWithMask(painter, xy, gradient(c, sz, Qt::Vertical, config.gradBtn), masks.radio);
      else
         fillWithMask(painter, xy, c, masks.radio);
      painter->save();
      painter->setBrush(Qt::NoBrush);
      painter->setPen(Qt::white);
      painter->setRenderHint(QPainter::Antialiasing);
      painter->drawEllipse(xy.x(), xy.y(), sz, sz);
      painter->restore();
      
      if (isOn) { // drop?
         xy += QPoint($2,$2);
         sz -= dpi.$4;
         QColor c = btnFgColor(PAL, isEnabled, hover||hasFocus);
         xy += QPoint($2,$2);
         fillWithMask(painter, xy, gradient(c, sz, Qt::Vertical, config.gradBtn), masks.radioIndicator);
      }
      break;
   }
   case PE_Q3DockWindowSeparator: // Item separator for Qt 3 compatible dock window and toolbar contents.
      break;
   case PE_Frame: { // Generic frame; see also QFrame.
      if (widget) {
         // handle the
         if (widget->inherits("QComboBoxPrivateContainer")) {
            SAVE_PEN;
            painter->setPen(COLOR(Base));
            painter->drawRect(RECT.adjusted(0,0,-1,-1));
            RESTORE_PEN;
            break;
         }
         
         bool inverse = true, niceFrame = false;
         QRect rect = RECT; const QRect *outerRect = 0;
         
         const Tile::Mask *mask = 0L; const Tile::Set *shadow = 0L;
         if (sunken) {
            shadow = &shadows.lineEdit[isEnabled];
            mask = &masks.button;
         }
         else if (option->state & State_Raised) {
            shadow = &shadows.tab;
            mask = &masks.tab;
         }
         QPoint zero;
         const QBrush *brush = &PAL.brush(widget->backgroundRole());
         if (qobject_cast<const QFrame*>(widget)) { // frame, can be killed unless...
            if (widget->inherits("QTextEdit")) { // ...it's a TextEdit!
               niceFrame = true;
               inverse = false; brush = &PAL.brush(QPalette::Base);
            }
            else { // maybe we need to corect a textlabels margin
               if (const QLabel* label = qobject_cast<const QLabel*>(widget))
                  if (label->text() != QString() && label->margin() < dpi.$3)
                     const_cast<QLabel*>(label)->setMargin(dpi.$3);
               break; // painted on visual frame
            }
         }
         else if (qobject_cast<const VisualFrame*>(widget)) {
            if (widget->parentWidget() && widget->parentWidget()->parentWidget())
               brush = &PAL.brush(widget->parentWidget()->parentWidget()->backgroundRole());
            niceFrame = true;
            zero = widget->mapTo(widget->topLevelWidget(), QPoint(0,0));
            if (!sunken) {
               outerRect = &RECT;
               if (option->state & State_Raised)
                  rect = RECT.adjusted(dpi.$2,dpi.$1,-dpi.$2,-dpi.$4);
               else
                  rect = RECT.adjusted(dpi.$2,dpi.$2,-dpi.$2,-dpi.$2);
            }
         }
         if (niceFrame) {
            if (mask)
               fillWithMask(painter, rect, *brush, mask, Tile::Full, false, zero, inverse, outerRect);
            if (hasFocus) {
               painter->save();
               painter->setPen(QPen(COLOR(Highlight), dpi.$2));
               painter->drawLine(rect.left()+dpi.$4, rect.bottom(), rect.right()-dpi.$3, rect.bottom());
               painter->restore();
            }
            if (shadow)
               shadow->render(RECT, painter);
            else { // plain frame
               //horizontal
               shadows.line[false][Sunken].render(RECT, painter, Tile::Full, false);
               shadows.line[false][Sunken].render(RECT, painter, Tile::Full, true);
               //vertical
               shadows.line[true][Sunken].render(RECT, painter, Tile::Full, false);
               shadows.line[true][Sunken].render(RECT, painter, Tile::Full, true);
               break;
            }
            break;
         }
      }
      // fallback, we cannot paint shaped frame contents
      if (sunken)
         shadows.sunken.render(RECT,painter);
      else if (option->state & State_Raised)
//          shadows.raised.render(RECT,painter);
         break;
      else {
         //horizontal
         shadows.line[false][Sunken].render(RECT, painter, Tile::Full, false);
         shadows.line[false][Sunken].render(RECT, painter, Tile::Full, true);
         //vertical
         shadows.line[true][Sunken].render(RECT, painter, Tile::Full, false);
         shadows.line[true][Sunken].render(RECT, painter, Tile::Full, true);
      }
      break;
   }
   case PE_FrameMenu: { // Frame for popup windows/menus; see also QMenu.
      SAVE_PEN;
    QRect frame = RECT;
    int x,y,w,h;
    frame.getRect(&x, &y, &w, &h);

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(Qt::NoBrush);
    QLinearGradient lg(0, 0, 0, 10);
    QGradientStops stops;
    stops << QGradientStop( 0, QColor(255,255,255, 110) )
           << QGradientStop( 1, QColor(128,128,128, 60) );
    lg.setStops(stops);
    painter->setPen(QPen(QBrush(lg),1));
    painter->drawLine(QPointF(6.3, 0.5), QPointF(w-6.3, 0.5));
    painter->drawArc(QRectF(0.5, 0.5, 9.5, 9.5),90*16, 90*16);
    painter->drawArc(QRectF(w-9.5-0.5, 0.5, 9.5, 9.5), 0, 90*16);

    painter->setPen(QColor(128,128,128, 60));
    painter->drawLine(QPointF(0.5, 6.3), QPointF(0.5, h-6.3));
    painter->drawLine(QPointF(w-0.5, 6.3), QPointF(w-0.5, h-6.3));

    lg = QLinearGradient(0, h-10, 0, h);
    stops.clear();
    stops << QGradientStop( 0, QColor(128,128,128, 60) )
           << QGradientStop( 1, QColor(0,0,0, 50) );
    lg.setStops(stops);
    painter->setPen(QPen(QBrush(lg),1));
    painter->drawArc(QRectF(0.5, h-9.5-0.5, 9.5, 9.5),180*16, 90*16);
    painter->drawArc(QRectF(w-9.5-0.5, h-9.5-0.5, 9.5, 9.5), 270*16, 90*16);
    painter->drawLine(QPointF(6.3, h-0.5), QPointF(w-6.3, h-0.5));

      RESTORE_PEN;
#if 0
      if (config.glassMenus) {
         painter->save();
         painter->setPen(Qt::white);
         painter->setBrush(Qt::NoBrush);
         painter->drawRect(RECT.adjusted(0,0,-1,-1));
         painter->restore();
//          painter->fillRect(RECT, Qt::white);
      }
      if (config.menuShadow) {
         QRect rect = RECT.adjusted(0,0, // don't ask...
                                    shadows.line[true][Sunken].thickness()+1,
                                    shadows.line[false][Sunken].thickness()+1);
         //horizontal
         shadows.line[false][Sunken].render(rect, painter, Tile::Full, false);
         shadows.line[false][Sunken].render(rect, painter, Tile::Full, true);
         //vertical
         shadows.line[true][Sunken].render(rect, painter, Tile::Full, false);
         shadows.line[true][Sunken].render(rect, painter, Tile::Full, true);
      }
#endif
     break;
   }
   case PE_PanelMenuBar: // Panel for menu bars.
   case PE_FrameDockWidget: // Panel frame for dock windows and toolbars.
      break;
   case PE_FrameTabWidget: // Frame for tab widgets.
      if (const QStyleOptionTabWidgetFrame *twf =
          qstyleoption_cast<const QStyleOptionTabWidgetFrame *>(option)) {
         
         bool mirror = false, vertical = false;
         switch (twf->shape) {
         case QTabBar::RoundedNorth: case QTabBar::TriangularNorth:
            break;
         case QTabBar::RoundedSouth: case QTabBar::TriangularSouth:
            mirror = true; break;
         case QTabBar::RoundedEast: case QTabBar::TriangularEast:
            mirror = true;
         case QTabBar::RoundedWest: case QTabBar::TriangularWest:
            vertical = true; break;
         }
             
         int baseHeight =
                vertical ? twf->tabBarSize.width() : twf->tabBarSize.height();
         if (baseHeight < 0)
            baseHeight = pixelMetric( PM_TabBarBaseHeight, option, widget )-dpi.$2;
         if (!baseHeight) {
            shadows.tab.render(RECT, painter);
            break;
         }
             
         QRect rect(RECT), tabRect(RECT), fillRect;
         int offset = 8;
         Qt::Orientation o = Qt::Vertical;
         Tile::PosFlags pf = Tile::Ring;
         if (vertical) { // east or west
            o = Qt::Horizontal;
            rect.adjust(0,offset,0,-offset);
            if (mirror) { // east
               rect.setLeft(rect.right()-baseHeight);
               fillRect = rect.adjusted(0, dpi.$2, -dpi.$3, -dpi.$3);
               pf &= ~Tile::Left;
               o = Qt::Horizontal;
               tabRect.setRight(tabRect.right()-(baseHeight-dpi.$2));
            }
            else {
               rect.setWidth(baseHeight);
               fillRect = rect.adjusted(dpi.$3, dpi.$2, 0, -dpi.$3);
               pf &= ~Tile::Right;
               o = Qt::Horizontal;
               tabRect.setLeft(tabRect.left()+(baseHeight-dpi.$2));
            }
            baseHeight = fillRect.width();
         }
         else { // north or south
            rect.adjust(offset,0,-offset,0);
            if (mirror) { //south
               rect.setTop(rect.bottom()-baseHeight);
               fillRect = rect.adjusted(dpi.$3, 0, -dpi.$3, -dpi.$3);
               pf &= ~Tile::Top;
               tabRect.setBottom(tabRect.bottom()-(baseHeight-dpi.$3));
            }
            else { // north
               rect.setHeight(baseHeight);
               fillRect = rect.adjusted(dpi.$3, dpi.$2, -dpi.$3, 0);
               pf &= ~Tile::Bottom;
               tabRect.setTop(tabRect.top()+baseHeight);
            }
            baseHeight = fillRect.height();
         }
         
         shadows.tab.render(rect, painter, pf);
         fillWithMask(painter, fillRect, gradient(CONF_COLOR(role_tab[0]), baseHeight, o, config.gradientStrong), &masks.tab, pf | Tile::Center);
         masks.tab.outline(fillRect, painter, CONF_COLOR(role_tab[0]).dark(110), true, pf);
         shadows.tab.render(tabRect, painter, Tile::Ring);
      }
      break;
   case PE_FrameLineEdit: // Panel frame for line edits.
      shadows.lineEdit[isEnabled].render(RECT.adjusted(0,0,0,-dpi.$2),painter);
      break;
   case PE_FrameGroupBox: { // Panel frame around group boxes.
      QRect rect = RECT.adjusted(dpi.$4,dpi.$2,-dpi.$4,0);
      rect.setHeight(qMax(dpi.$32, RECT.height()));
      fillWithMask(painter, rect, groupLight(rect.height()), &masks.button,
                   Tile::Full&~Tile::Bottom);
      rect.setBottom(RECT.bottom()-dpi.$32);
      shadows.group.render(RECT, painter, Tile::Ring);
      masks.button.outline(rect, painter, COLOR(Window).light(120), true,
                           Tile::Full&~Tile::Bottom);
      break;
   }
//    case PE_FrameButtonBevel: // Panel frame for a button bevel
//    case PE_FrameButtonTool: // Panel frame for a tool button
   case PE_FrameStatusBar: // Frame for a section of a status bar; see also QStatusBar.
      break;
   case PE_FrameWindow: // Frame around a MDI window or a docking window.
   {
      painter->save();
      painter->setPen(PAL.color(QPalette::Window).dark(110));
      painter->drawRect(RECT);
      painter->restore();
      break;
   }
   case PE_Q3Separator: // Qt 3 compatible generic separator.
      break;
   case PE_IndicatorViewItemCheck: // On/off indicator for a view item
   case PE_Q3CheckListIndicator: // Qt 3 compatible Checkbox part of a list view item.
   case PE_IndicatorMenuCheckMark: { // Check mark used in a menu.
      QRect rect;
      if (RECT.width() > RECT.height())
         rect.setRect(RECT.x()+(RECT.width()-RECT.height())/2, RECT.y(),
                      RECT.height()-1, RECT.height()-1);
      else
         rect.setRect(RECT.x(), RECT.y()+(RECT.height()-RECT.width())/2,
                      RECT.width()-1, RECT.width()-1);
      int off = rect.width()/4;
      
      painter->save();
      const bool selected = option->state & State_Selected && isEnabled;
      painter->setBrush(Qt::NoBrush);
      QPalette::ColorRole fgr;
      if (pe == PE_IndicatorMenuCheckMark) {
         if (widget)
            fgr = selected ? widget->backgroundRole() : widget->foregroundRole();
         else
            fgr = selected ? QPalette::Window : QPalette::WindowText;
      }
      else
         fgr = selected ? QPalette::HighlightedText : QPalette::Text;
      
      painter->setPen(PAL.color(fgr));
      painter->drawRect(rect.adjusted(0, off, -off, 0));
      
      if (!(option->state & State_Off)) {
         painter->setRenderHint(QPainter::Antialiasing);
         if (!(pe == PE_IndicatorMenuCheckMark || selected)) {
            fgr = QPalette::Highlight;
            painter->setPen(PAL.color(fgr));
         }
         painter->setBrush(PAL.color(fgr));
         const QPoint points[4] = {
            QPoint(rect.right(), rect.top()),
            QPoint(rect.x()+rect.width()/3, rect.bottom()),
            QPoint(rect.x(), rect.bottom()-rect.height()/3),
               QPoint(rect.x()+rect.width()/3, rect.bottom()-rect.height()/5)
         };
         painter->drawPolygon(points, 4);
      }
      painter->restore();
      break;
   }
   case PE_Q3CheckListExclusiveIndicator: // Qt 3 compatible Radio button part of a list view item.
      painter->save();
      painter->setRenderHint ( QPainter::Antialiasing );
      painter->drawEllipse ( RECT );
      _PRINTFLAGS_;
      if (option->state & State_On) {
         painter->setBrush ( painter->pen().color() );
         painter->drawEllipse ( RECT.adjusted(RECT.width()/4, RECT.height()/4, -RECT.width()/4, -RECT.height()/4) );
      }
      painter->restore();
      break;
//    case PE_IndicatorProgressChunk: // Section of a progress bar indicator; see also QProgressBar.
//    case PE_Q3CheckListController: // Qt 3 compatible Controller part of a list view item.
   case PE_IndicatorBranch: // Lines used to represent the branch of a tree in a tree view.
   {
      SAVE_PEN;
      int mid_h = RECT.x() + RECT.width() / 2;
      int mid_v = RECT.y() + RECT.height() / 2;
      int bef_h = mid_h;
      int bef_v = mid_v;
      int aft_h = mid_h;
      int aft_v = mid_v;
      
      painter->setPen(widget ?
                      midColor( PAL.color(widget->backgroundRole()), PAL.color(widget->foregroundRole())) :
                      midColor( PAL.color(QPalette::Base), PAL.color(QPalette::Text)) );
      static const int decoration_size = 9;
      if (option->state & State_Children) {
         int delta = decoration_size / 2 + 2;
         bef_h -= delta;
         bef_v -= delta;
         aft_h += delta;
         aft_v += delta;
         QStyleOption tmpOpt = *option;
         tmpOpt.rect = QRect(bef_h+2, bef_v+2, decoration_size, decoration_size);
         drawPrimitive(option->state & State_Open ? PE_IndicatorArrowDown :
                       option->direction == Qt::RightToLeft ? PE_IndicatorArrowLeft :
                       PE_IndicatorArrowRight, &tmpOpt, painter, widget);
      }
      if (RECT.x() ==  -1) { // this is for the first col and i don't see why we'd need a line here
         RESTORE_PEN;
         break;
      }
      
      if (option->state & (State_Item | State_Sibling))
         painter->drawLine(mid_h, RECT.y(), mid_h, bef_v);
      if (option->state & State_Sibling)
         painter->drawLine(mid_h, aft_v, mid_h, RECT.bottom());
      if (option->state & State_Item) {
         if (option->direction == Qt::RightToLeft)
            painter->drawLine(RECT.left(), mid_v, bef_h, mid_v);
         else
            painter->drawLine(aft_h, mid_v, RECT.right(), mid_v);
      }
      RESTORE_PEN;
      break;
   }
   case PE_IndicatorDockWidgetResizeHandle: // Resize handle for dock windows.
   {
      QPoint *points; int num;
      const int $12 = dpi.$12, $6 = dpi.$6;
      if (RECT.width() > RECT.height()) {
         int x = RECT.left()+RECT.width()/3;
         int y = RECT.top()+(RECT.height()-$6)/2;
         num = RECT.width()/(3*$12);
         if ((RECT.width()/3) % $12) ++num;
         points = new QPoint[num];
         for (int i = 0; i < num; ++i) {
            points[i] = QPoint(x,y); x += $12;
         }
      }
      else {
         int x = RECT.left()+(RECT.width()-$6)/2;
         int y = RECT.top()+RECT.height()/3;
         num = RECT.height()/(3*$12);
         if ((RECT.height()/3) % $12) ++num;
         points = new QPoint[num];
         for (int i = 0; i < num; ++i) {
            points[i] = QPoint(x,y); y += $12;
         }
      }
      painter->save();
      painter->setPen(Qt::NoPen);
      const QPixmap *fill; int cnt = num/2, imp = 1;
      QPalette::ColorRole role = QPalette::WindowText;
      if (hover) {
         role = QPalette::Highlight;
         imp = 8;
      }
      if (num%2)
      {
         fill = &gradient(midColor(COLOR(Window), PAL.color(role), 3, imp), $6, Qt::Vertical, GradSunken);
         fillWithMask(painter, points[cnt], *fill, masks.notch);
      }
      --num;
      for (int i = 0; i < cnt; ++i)
      {
         fill = &gradient(midColor(COLOR(Window), PAL.color(role), 3+cnt-i, imp), $6, Qt::Vertical, GradSunken);
         fillWithMask(painter, points[i], *fill, masks.notch);
         fillWithMask(painter, points[num-i], *fill, masks.notch);
      }
      painter->restore();
      delete[] points;
      break;
   }
   case PE_IndicatorToolBarHandle: // The handle of a toolbar.
      if (!(widget && widget->parentWidget()) ||
          widget->parentWidget()->underMouse()) {
         painter->save();
         QRect rect = RECT; bool line = false; int dx(0), dy(0);
         if (RECT.width() > RECT.height()) {
            line = (RECT.width() > 9*RECT.height()/2);
            if (line) {
               dx = 3*RECT.height()/2; dy = 0;
            }
            rect.setLeft(rect.left()+(rect.width()-rect.height())/2);
            rect.setWidth(rect.height());
         }
         else {
            line = (RECT.height() > 3*RECT.width());
            if (line) {
               dx = 0; dy = 3*RECT.width()/2;
            }
            rect.setTop(rect.top()+(rect.height()-rect.width())/2);
            rect.setHeight(rect.width());
         }
         QColor c = hover?COLOR(Highlight):COLOR(Window).dark(110);
         painter->setRenderHint(QPainter::Antialiasing);
         painter->setBrush(gradient(c, rect.height(), Qt::Vertical, GradSunken));
         painter->setPen(Qt::NoPen);
         painter->setBrushOrigin(rect.topLeft());
         painter->drawEllipse(rect);
         if (line) {
            const int $1 = dpi.$1;
            rect.adjust($1,$1,-$1,-$1);
            painter->setBrush(gradient(c, rect.height(), Qt::Vertical, GradSunken));
            rect.translate(-dx,-dy);
            painter->setBrushOrigin(rect.topLeft());
            painter->drawEllipse(rect);
            rect.translate( 2*dx, 2*dy);
            painter->setBrushOrigin(rect.topLeft());
            painter->drawEllipse(rect);
         }
         painter->restore();
      }
      break;
   case PE_IndicatorToolBarSeparator: // The separator in a toolbar.
      break;
   case PE_PanelToolBar: // The panel for a toolbar.
      break;
//    case PE_PanelTipLabel: // The panel for a tip label.
   case PE_FrameTabBarBase: // The frame that is drawn for a tabbar, ususally drawn for a tabbar that isn't part of a tab widget
      if (widget &&
          (qobject_cast<const QTabBar*>(widget) || // we alter the paintevent
          (widget->parentWidget() &&
           qobject_cast<QTabWidget*>(widget->parentWidget())))) // KDE abuse, allready has a nice base
         break;
      
      if (const QStyleOptionTabBarBase *tbb
            = qstyleoption_cast<const QStyleOptionTabBarBase *>(option)) {
         int size(0); Qt::Orientation o = Qt::Vertical;
         Tile::PosFlags pf = Tile::Ring;
         QRect fillRect;
         bool north = false;
         switch (tbb->shape) {
         case QTabBar::RoundedSouth:
         case QTabBar::TriangularSouth:
//             fillRect = RECT.adjusted(dpi.$3, dpi.$2, -dpi.$3, 0);
            pf &= ~Tile::Top;
            size = /*fillRect*/RECT.height();
            break;
         case QTabBar::RoundedNorth:
         case QTabBar::TriangularNorth:
            north = true;
//             fillRect = RECT.adjusted(dpi.$3, 0, -dpi.$3, -dpi.$3);
            pf &= ~Tile::Bottom;
            size = /*fillRect*/RECT.height();
            break;
         case QTabBar::RoundedWest:
         case QTabBar::TriangularWest:
//             fillRect = RECT.adjusted(0, dpi.$2, -dpi.$3, -dpi.$3);
            pf &= ~Tile::Right;
            o = Qt::Horizontal;
            size = /*fillRect*/RECT.width();
            break;
         case QTabBar::RoundedEast:
         case QTabBar::TriangularEast:
//             fillRect = RECT.adjusted(dpi.$3, dpi.$2, 0, -dpi.$3);
            pf &= ~Tile::Left;
            o = Qt::Horizontal;
            size = /*fillRect*/RECT.width();
            break;
         }
         fillWithMask(painter, RECT,
                      gradient(CONF_COLOR(role_tab[0]), size, o, config.gradientStrong),
                      &masks.button, pf | Tile::Center);
         if (north)
            shadows.lineEdit[0].render(RECT, painter, Tile::Ring & ~Tile::Bottom);
         else
            shadows.tab.render(RECT, painter, pf);
//          masks.tab.outline(fillRect, painter, CONF_COLOR(role_tab[0]).dark(110), true, pf);
      }
      break;
   case PE_IndicatorTabTear: // An indicator that a tab is partially scrolled out of the visible tab bar when there are many tabs.
      break;
   default:
      QCommonStyle::drawPrimitive( pe, option, painter, widget );
   } // switch
}
