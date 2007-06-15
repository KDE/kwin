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

#include <QComboBox>
#include <QPainter>
#include <QTime>
// #include <QPixmapCache>
#include <QStyleOptionComplex>
#include <cmath>
#include "oxygen.h"

using namespace Oxygen;

extern Config config;
extern Dpi dpi;

#include "inlinehelp.cpp"
#include "makros.h"

void OxygenStyle::drawComplexControl ( ComplexControl control, const QStyleOptionComplex * option, QPainter * painter, const QWidget * widget) const
{
   Q_ASSERT(option);
   Q_ASSERT(painter);
   
   bool sunken = option->state & State_Sunken;
   bool isEnabled = option->state & State_Enabled;
   bool hover = isEnabled && (option->state & State_MouseOver);
   bool hasFocus = option->state & State_HasFocus;
   
   switch ( control )
   {
   case CC_SpinBox: // A spinbox, like QSpinBox
      if (const QStyleOptionSpinBox *sb =
          qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
         QStyleOptionSpinBox copy = *sb;
         int uh = 0;
         
         if (sb->frame && (sb->subControls & SC_SpinBoxFrame))
            drawPrimitive ( PE_PanelLineEdit, sb, painter, widget );
         
         if (!isEnabled)
            break; // why bother the user with elements he can't use... ;)
         
         Tile::PosFlags pf;
         
         if (sb->subControls & SC_SpinBoxUp) {
            copy.subControls = SC_SpinBoxUp;
            copy.rect = subControlRect(CC_SpinBox, sb, SC_SpinBoxUp, widget);
            uh = copy.rect.height();
            
            pf = Tile::Top | Tile::Left | Tile::Right;
            isEnabled = sb->stepEnabled & QAbstractSpinBox::StepUpEnabled;
            hover = isEnabled && (sb->activeSubControls == SC_SpinBoxUp);
            sunken = sunken && (sb->activeSubControls == SC_SpinBoxUp);
            
            int dx = copy.rect.width()/4, dy = copy.rect.height()/4;
            copy.rect.adjust(dx, 2*dy,-dx,-dpi.$1);
            
            QColor c;
            if (hover)
               c = COLOR(Highlight);
            else if (isEnabled)
               c = COLOR(Text);
            else
               c = midColor(COLOR(Base), QPalette::Text);
            
            painter->setPen(c);
            drawPrimitive(PE_IndicatorSpinUp, &copy, painter, widget);
         }
         
         if (sb->subControls & SC_SpinBoxDown) {
            copy.subControls = SC_SpinBoxDown;
            copy.rect = subControlRect(CC_SpinBox, sb, SC_SpinBoxDown, widget);
            
            pf = Tile::Bottom | Tile::Left | Tile::Right;
            isEnabled = sb->stepEnabled & QAbstractSpinBox::StepDownEnabled;
            hover = isEnabled && (sb->activeSubControls == SC_SpinBoxDown);
            sunken = sunken && (sb->activeSubControls == SC_SpinBoxDown);
            
            int dx = copy.rect.width()/4, dy = copy.rect.height()/4;
            copy.rect.adjust(dx, dpi.$1,-dx,-2*dy);
            
            QColor c;
            if (hover)
               c = COLOR(Highlight);
            else if (isEnabled)
               c = COLOR(Text);
            else
               c = midColor(COLOR(Base), QPalette::Text);
            
            painter->setPen(c);
            drawPrimitive(PE_IndicatorSpinDown, &copy, painter, widget);
         }
      }
      break;
   case CC_GroupBox:
      if (const QStyleOptionGroupBox *groupBox =
          qstyleoption_cast<const QStyleOptionGroupBox *>(option)) {
         if (groupBox->subControls & QStyle::SC_GroupBoxFrame) {
            QStyleOptionFrameV2 frame;
            frame.QStyleOption::operator=(*groupBox);
            frame.features = groupBox->features;
            frame.lineWidth = groupBox->lineWidth;
            frame.midLineWidth = groupBox->midLineWidth;
            frame.rect = subControlRect(CC_GroupBox, option, SC_GroupBoxFrame, widget);
            drawPrimitive(PE_FrameGroupBox, &frame, painter, widget);
         }
         
         // Draw title
         if ((groupBox->subControls & QStyle::SC_GroupBoxLabel) &&
             !groupBox->text.isEmpty()) {
            QColor textColor = groupBox->textColor;
            if (textColor.isValid()) painter->setPen(textColor);
            QFont tmpfnt = painter->font(); tmpfnt.setBold(true);
            painter->setFont ( tmpfnt );
            QStyleOptionGroupBox copy = *groupBox; copy.fontMetrics = QFontMetrics(tmpfnt);
            QRect textRect = subControlRect(CC_GroupBox, &copy, SC_GroupBoxLabel, widget);
            int alignment = Qt::AlignCenter; //int(groupBox->textAlignment);
            if (!styleHint(QStyle::SH_UnderlineShortcut, option, widget))
               alignment |= Qt::TextHideMnemonic;
            else
               alignment |= Qt::TextShowMnemonic;
            
            drawItemText(painter, textRect,  alignment, groupBox->palette, isEnabled, groupBox->text,
                           textColor.isValid() ? QPalette::NoRole : QPalette::Foreground);
            int x = textRect.bottom(); textRect = RECT; textRect.setTop(x);
            x = textRect.width()/4; textRect.adjust(x,0,-x,0);
            shadows.line[0][Sunken].render(textRect, painter);
         }
         
         // Draw checkbox // TODO: sth better - maybe a round thing in the upper left corner...? also doesn't hover - yet.
         if (groupBox->subControls & SC_GroupBoxCheckBox) {
            QStyleOptionButton box;
            box.QStyleOption::operator=(*groupBox);
            box.rect = subControlRect(CC_GroupBox, option, SC_GroupBoxCheckBox, widget);
            drawPrimitive(PE_IndicatorCheckBox, &box, painter, widget);
         }
      }
      break;
   case CC_ComboBox: // A combobox, like QComboBox
      if (const QStyleOptionComboBox *cmb =
          qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
         QRect ar, r = RECT.adjusted(0,0,0,-dpi.$2);
         const QComboBox* combo = widget ?
                qobject_cast<const QComboBox*>(widget) : 0;
         const bool listShown = combo && combo->view() &&
               ((QWidget*)(combo->view()))->isVisible();
         const bool reverse = (option->direction == Qt::RightToLeft);
         
         // do we have an arrow?
         if ((cmb->subControls & SC_ComboBoxArrow))
            ar = subControlRect(CC_ComboBox, cmb, SC_ComboBoxArrow, widget);

         // the label
         if ((cmb->subControls & SC_ComboBoxFrame) && cmb->frame) {
            if (cmb->editable)
               drawPrimitive(PE_PanelLineEdit, option, painter, widget);
            else {
               drawPrimitive(PE_PanelButtonBevel, option, painter, widget);
            }
         }

         // the arrow
         if (!ar.isNull()) {
            ar.adjust(ar.width()/3,ar.height()/3,-ar.width()/3,-ar.height()/3);
            QStyleOptionComboBox tmpOpt = *cmb;
            PrimitiveElement arrow;
            if (!listShown)
               arrow = PE_IndicatorArrowDown;
            else if (reverse)
               arrow = PE_IndicatorArrowRight;
            else
               arrow = PE_IndicatorArrowLeft;
            if (cmb->editable)
               hover = hover && (cmb->activeSubControls == SC_ComboBoxArrow);
            painter->setRenderHint ( QPainter::Antialiasing, true );
            painter->save();

            painter->setPen(COLOR(Text));
            if (hover || listShown) {
               if (cmb->editable)
                  painter->setPen(COLOR(Highlight));
            }
            tmpOpt.rect =  ar;
            drawPrimitive(arrow, &tmpOpt, painter, widget);
            painter->restore();
         }
      }
      break;
   case CC_ScrollBar: // A scroll bar, like QScrollBar
      if (const QStyleOptionSlider *scrollbar =
         qstyleoption_cast<const QStyleOptionSlider *>(option)) {
         // Make a copy here and reset it for each primitive.
         QStyleOptionSlider newScrollbar = *scrollbar;
         
         // TODO: this is a stupid hack, move the whole special scrollbar painting here!
         if (widget && widget->parentWidget() &&
              widget->parentWidget()->parentWidget() &&
              widget->parentWidget()->parentWidget()->inherits("QComboBoxListView")) {
            painter->fillRect(RECT, PAL.brush(QPalette::Base));
            newScrollbar.state |= State_Item;
         }
         
         State saveFlags = newScrollbar.state;
         if (scrollbar->minimum == scrollbar->maximum)
               saveFlags &= ~State_Enabled;
         
         SubControls hoverControls = scrollbar->activeSubControls &
               (SC_ScrollBarSubLine | SC_ScrollBarAddLine | SC_ScrollBarSlider);
         const ComplexHoverFadeInfo *info =
               complexHoverFadeInfo(widget, hoverControls);
         
#define PAINT_ELEMENT(_SC_, _CE_)\
         if (scrollbar->subControls & _SC_) {\
            newScrollbar.rect = scrollbar->rect;\
            newScrollbar.state = saveFlags;\
            newScrollbar.rect = subControlRect(control, &newScrollbar, _SC_, widget);\
            if (newScrollbar.rect.isValid()) {\
               if (!(scrollbar->activeSubControls & _SC_))\
                  newScrollbar.state &= ~(State_Sunken | State_MouseOver);\
               if (info && (info->fadingInControls & _SC_ || info->fadingOutControls & _SC_))\
                  const_cast<OxygenStyle*>( this )->complexStep = info->steps.value(_SC_); \
               else \
                  const_cast<OxygenStyle*>( this )->complexStep = 0; \
               drawControl(_CE_, &newScrollbar, painter, widget);\
            }\
         }//
         
         PAINT_ELEMENT(SC_ScrollBarSubLine, CE_ScrollBarSubLine);
         PAINT_ELEMENT(SC_ScrollBarAddLine, CE_ScrollBarAddLine);
         PAINT_ELEMENT(SC_ScrollBarSubPage, CE_ScrollBarSubPage);
         PAINT_ELEMENT(SC_ScrollBarAddPage, CE_ScrollBarAddPage);
//          PAINT_ELEMENT(SC_ScrollBarFirst, CE_ScrollBarFirst);
//          PAINT_ELEMENT(SC_ScrollBarLast, CE_ScrollBarLast);
         
         if (scrollbar->subControls & SC_ScrollBarSlider) {
            newScrollbar.rect = scrollbar->rect;
            newScrollbar.state = saveFlags;
            newScrollbar.rect = subControlRect(control, &newScrollbar,
                                               SC_ScrollBarSlider, widget);
            if (newScrollbar.rect.isValid()) {
               if (!(scrollbar->activeSubControls & SC_ScrollBarSlider))
                  newScrollbar.state &= ~(State_Sunken | State_MouseOver);
               if (scrollbar->state & State_HasFocus)
                  newScrollbar.state |= (State_Sunken | State_MouseOver);
               if (info && (info->fadingInControls & SC_ScrollBarSlider ||
                   info->fadingOutControls & SC_ScrollBarSlider))
                  const_cast<OxygenStyle*>( this )->complexStep =
                        info->steps.value(SC_ScrollBarSlider);
               else
                  const_cast<OxygenStyle*>( this )->complexStep = 0;
               drawControl(CE_ScrollBarSlider, &newScrollbar, painter, widget);
            }
         }
      }
      break;
   case CC_Slider: // A slider, like QSlider
      if (const QStyleOptionSlider *slider =
          qstyleoption_cast<const QStyleOptionSlider *>(option)) {
         QRect groove = QCommonStyle::subControlRect(CC_Slider, slider, SC_SliderGroove, widget);
         QRect handle = QCommonStyle::subControlRect(CC_Slider, slider, SC_SliderHandle, widget);

         isEnabled = isEnabled && (slider->maximum > slider->minimum);
         hover = isEnabled && hover && (slider->activeSubControls & SC_SliderHandle);
         sunken = sunken && (slider->activeSubControls & SC_SliderHandle);
         
         const int ground = 0;
         
         if ((slider->subControls & SC_SliderGroove) &&
             groove.isValid()) {
            QRect r;
            QColor c = btnBgColor(PAL, isEnabled, hasFocus);
            if ( slider->orientation == Qt::Horizontal ) {
               // the groove
               groove.adjust(0,handle.height()/3,0,-handle.height()/3);
               fillWithMask(painter, groove, gradient(COLOR(Window),
                            groove.height(), Qt::Vertical, GradSunken),
                            &masks.button);
#if 0
               // the "temperature"
               if (slider->sliderPosition != ground &&
                   slider->maximum > slider->minimum) {
                  groove.adjust(0,dpi.$1,0,-dpi.$1);
                  int groundX = groove.width() * (ground - slider->minimum) /
                        (slider->maximum - slider->minimum);
                  bool rightSide = slider->sliderPosition > ground;
                  if (slider->upsideDown) {
                     rightSide = !rightSide;
                     groundX = groove.right() - groundX;
                  }
                  else
                     groundX += groove.left();
                  
                  if (rightSide) {
                     groove.setLeft(groundX);
                     groove.setRight(handle.center().x());
                  }
                  else {
                     groove.setLeft(handle.center().x());
                     groove.setRight(groundX);
                  }
                  fillWithMask(painter, groove, gradient(COLOR(Window),
                               groove.height(), Qt::Vertical, config.gradient),
                               &masks.button);
               }
#else
               groove.adjust(0,dpi.$1,0,-dpi.$1);
               fillWithMask(painter, groove, gradient(COLOR(Window),
                               groove.height(), Qt::Vertical, config.gradient),
                               &masks.button);
#endif
               // for later (cosmetic)
               handle.translate(0,dpi.$3);
            }
            else { // Vertical
               // the groove
               groove.adjust(handle.width()/3,0,-handle.width()/3,0);
               fillWithMask(painter, groove, gradient(COLOR(Window),
                            groove.width(), Qt::Horizontal, GradSunken),
                            &masks.button);
#if 0
               // the "temperature"
               if (slider->sliderPosition != ground &&
                   slider->maximum > slider->minimum) {
                  groove.adjust(dpi.$1,0,-dpi.$1,0);
                  int groundY = groove.height() * (ground - slider->minimum) /
                        (slider->maximum - slider->minimum);
                  bool upside = slider->sliderPosition > ground;
                  if (slider->upsideDown) {
                     upside = !upside;
                     groundY = groove.bottom() - groundY;
                  }
                  else
                     groundY += groove.top();
                  
                  if (upside) {
                     groove.setBottom(handle.center().y());
                     groove.setTop(groundY);
                  }
                  else {
                     groove.setBottom(groundY);
                     groove.setTop(handle.center().y());
                  }
                  fillWithMask(painter, groove, gradient(COLOR(Window),
                               groove.width(), Qt::Horizontal, config.gradient),
                               &masks.button);
               }
#else
               groove.adjust(dpi.$1,0,-dpi.$1,0);
               fillWithMask(painter, groove, gradient(COLOR(Window),
                               groove.width(), Qt::Horizontal, config.gradient),
                               &masks.button);
#endif
               // for later (cosmetic)
               handle.translate(dpi.$3,0);
            }
         }
         
         int direction = 0;
         if (slider->orientation == Qt::Vertical)
            ++direction;
         
         // ticks - TODO: paint our own ones?
         if ((slider->subControls & SC_SliderTickmarks) &&
              (slider->tickPosition != QSlider::NoTicks) ) {
            if (slider->tickPosition == QSlider::TicksAbove) {
               direction += 2;
               if (slider->orientation == Qt::Horizontal)
                  handle.translate(0,-dpi.$6);
               else
                  handle.translate(-dpi.$6,0);
            }
            QStyleOptionSlider tmpSlider = *slider;
            tmpSlider.subControls = SC_SliderTickmarks;
            QCommonStyle::drawComplexControl(control, &tmpSlider, painter, widget);
         }
         
         // handle
         if (slider->subControls & SC_SliderHandle) {
            int step = 0;
            if (!hasFocus) {
               const ComplexHoverFadeInfo *info =
                     complexHoverFadeInfo(widget, slider->activeSubControls & SC_SliderHandle);
               if (info && (info->fadingInControls & SC_SliderHandle ||
                   info->fadingOutControls & SC_SliderHandle))
                  step = info->steps.value(SC_SliderHandle);
            }
            // shadow
            QPoint xy = handle.topLeft();
            painter->drawPixmap(sunken ? xy + QPoint(dpi.$1,dpi.$1) : xy,
                                shadows.slider[direction][sunken][hover]);
            // gradient
            xy += QPoint(dpi.$2, direction?dpi.$1:0);
            fillWithMask(painter, xy, gradient(btnBgColor(PAL, isEnabled,
                         hover || hasFocus, step), dpi.SliderControl-dpi.$4,
                         Qt::Vertical, config.gradBtn), masks.slider[direction]);
            painter->drawPixmap(xy, lights.slider[direction]);
//             SAVE_PEN;
//             painter->setPen(btnFgColor(PAL, isEnabled, hover || hasFocus, step));
//             painter->drawPoint(handle.center());
//             RESTORE_PEN;
         }
      }
      break;
   case CC_ToolButton: // A tool button, like QToolButton
      // special handling for the tabbar scrollers ----------------------------------
      if (widget && widget->parentWidget() &&
          qobject_cast<QTabBar*>(widget->parent())) {
         QColor c = widget->parentWidget()->palette().color(config.role_tab[0]);
         QColor c2 = widget->parentWidget()->palette().color(config.role_tab[1]);
         if (sunken) {
            int dy = (RECT.height()-RECT.width())/2;
            QRect r = RECT.adjusted(dpi.$2,dy,-dpi.$2,-dy);
            painter->drawTiledPixmap(r, gradient(c, r.height(), Qt::Vertical, GradSunken));
         }
         painter->save();
         painter->setPen( isEnabled ? c2 : midColor(c, c2) );
         drawControl(CE_ToolButtonLabel, option, painter, widget);
         painter->restore();
         break;
      }
      // --------------------------------------------------------------------
      
      if (const QStyleOptionToolButton *toolbutton
          = qstyleoption_cast<const QStyleOptionToolButton *>(option)) {
         QRect menuarea = subControlRect(control, toolbutton, SC_ToolButtonMenu, widget);
         QRect button = subControlRect(control, toolbutton, SC_ToolButton, widget);
         State bflags = toolbutton->state;
             
         if ((bflags & State_AutoRaise) && !hover)
            bflags &= ~State_Raised;
         
         State mflags = bflags;

         if (toolbutton->activeSubControls & SC_ToolButton)
            bflags |= State_Sunken;
         
         hover = isEnabled && (bflags & (State_Sunken | State_On | State_Raised | State_HasFocus));

         QStyleOption tool(0); tool.palette = toolbutton->palette;
         
         // frame around whole button
         if (hover) {
            tool.rect = RECT; tool.state = bflags;
            drawPrimitive(PE_PanelButtonTool, &tool, painter, widget);
         }
         
         // don't paint a dropdown arrow iff the button's really pressed
         if (!(bflags & State_Sunken) &&
             (toolbutton->subControls & SC_ToolButtonMenu)) {
            if (toolbutton->activeSubControls & SC_ToolButtonMenu)
               painter->drawTiledPixmap(menuarea, gradient(COLOR(Window),
                                        menuarea.height(), Qt::Vertical,
                                        GradSunken));
            QPen oldPen = painter->pen();
            painter->setPen(midColor(COLOR(Window), COLOR(WindowText), 2, 1));
            tool.rect = menuarea; tool.state = mflags;
            drawPrimitive(PE_IndicatorButtonDropDown, &tool, painter, widget);
            painter->setPen(oldPen);
            if (hover) {
               menuarea.setLeft(button.right()-shadows.line[1][Sunken].thickness()/2);
               shadows.line[1][Sunken].render(menuarea, painter);
            }
         }
         
         // label in the toolbutton area
         QStyleOptionToolButton label = *toolbutton;
         label.rect = button;
         drawControl(CE_ToolButtonLabel, &label, painter, widget);
      }
      break;
   case CC_TitleBar: // A Title bar, like what is used in Q3Workspace
      if (const QStyleOptionTitleBar *tb =
          qstyleoption_cast<const QStyleOptionTitleBar *>(option))
      {
         painter->fillRect(RECT, PAL.brush(QPalette::Window));
         QRect ir;
         if (option->subControls & SC_TitleBarLabel)
         {
            ir = subControlRect(CC_TitleBar, tb, SC_TitleBarLabel, widget);
            painter->setPen(PAL.color(QPalette::WindowText));
            painter->drawText(ir.x() + dpi.$2, ir.y(), ir.width() - dpi.$2, ir.height(), Qt::AlignCenter | Qt::TextSingleLine, tb->text);
         }
         
         
#define PAINT_WINDOW_BUTTON(_scbtn_, _spbtn_)\
         {\
            tmpOpt.rect = subControlRect(CC_TitleBar, tb, _scbtn_, widget);\
            if (tb->activeSubControls & _scbtn_)\
               tmpOpt.state = tb->state;\
            else\
               tmpOpt.state &= ~(State_Sunken | State_MouseOver);\
            pm = standardPixmap ( _spbtn_, &tmpOpt, widget );\
            painter->drawPixmap(tmpOpt.rect.topLeft(), pm);\
         }
         
         QPixmap pm;
         QStyleOptionTitleBar tmpOpt = *tb;
         if (tb->subControls & SC_TitleBarCloseButton)
            PAINT_WINDOW_BUTTON(SC_TitleBarCloseButton, SP_TitleBarCloseButton)
         
         if (tb->subControls & SC_TitleBarMaxButton
             && tb->titleBarFlags & Qt::WindowMaximizeButtonHint)
         {
            if (tb->titleBarState & Qt::WindowMaximized)
               PAINT_WINDOW_BUTTON(SC_TitleBarNormalButton, SP_TitleBarNormalButton)
            else
               PAINT_WINDOW_BUTTON(SC_TitleBarMaxButton, SP_TitleBarMaxButton)
         }
         
         if (tb->subControls & SC_TitleBarMinButton
             && tb->titleBarFlags & Qt::WindowMinimizeButtonHint)
         {
            if (tb->titleBarState & Qt::WindowMinimized)
               PAINT_WINDOW_BUTTON(SC_TitleBarNormalButton, SP_TitleBarNormalButton)
            else
               PAINT_WINDOW_BUTTON(SC_TitleBarMinButton, SP_TitleBarMinButton)
         }
         
         if (tb->subControls & SC_TitleBarNormalButton &&
             tb->titleBarFlags & Qt::WindowMinMaxButtonsHint)
            PAINT_WINDOW_BUTTON(SC_TitleBarNormalButton, SP_TitleBarNormalButton)
         
         if (tb->subControls & SC_TitleBarShadeButton)
            PAINT_WINDOW_BUTTON(SC_TitleBarShadeButton, SP_TitleBarShadeButton)
         
         if (tb->subControls & SC_TitleBarUnshadeButton)
            PAINT_WINDOW_BUTTON(SC_TitleBarUnshadeButton, SP_TitleBarUnshadeButton)
         
         if (tb->subControls & SC_TitleBarContextHelpButton
             && tb->titleBarFlags & Qt::WindowContextHelpButtonHint)
            PAINT_WINDOW_BUTTON(SC_TitleBarContextHelpButton, SP_TitleBarContextHelpButton)
         
         if (tb->subControls & SC_TitleBarSysMenu
             && tb->titleBarFlags & Qt::WindowSystemMenuHint)
         {
            if (!tb->icon.isNull())
            {
               ir = subControlRect(CC_TitleBar, tb, SC_TitleBarSysMenu, widget);
               tb->icon.paint(painter, ir);
            }
            else
               PAINT_WINDOW_BUTTON(SC_TitleBarSysMenu, SP_TitleBarMenuButton)
         }
      }
      break;
//    case CC_Q3ListView: // Used for drawing the Q3ListView class
   case CC_Dial: // A dial, like QDial
      if (const QStyleOptionSlider *dial =
          qstyleoption_cast<const QStyleOptionSlider *>(option)) {
         painter->save();
         QRect rect = RECT;
         if (rect.width() > rect.height()) {
            rect.setLeft(rect.x()+(rect.width()-rect.height())/2); rect.setWidth(rect.height());
         }
         else {
            rect.setTop(rect.y()+(rect.height()-rect.width())/2); rect.setHeight(rect.width());
         }
         
         int d = qMax(rect.width()/6, dpi.$10);
         int r = (rect.width()-d)/2;
         qreal a;
         if (dial->maximum == dial->minimum)
            a = M_PI / 2;
         else if (dial->dialWrapping)
            a = M_PI * 3 / 2 - (dial->sliderValue - dial->minimum) * 2 * M_PI
            / (dial->maximum - dial->minimum);
         else
            a = (M_PI * 8 - (dial->sliderValue - dial->minimum) * 10 * M_PI
                  / (dial->maximum - dial->minimum)) / 6;
         
         QPoint cp((int)(r * cos(a)), -(int)(r * sin(a)));
         cp += rect.center();
         
         // the huge ring
         r = d/2; rect.adjust(r,r,-r,-r);
         painter->setPen(COLOR(Window).dark(115));
         painter->setRenderHint( QPainter::Antialiasing );
         painter->drawEllipse(rect);
         rect.translate(0, 1);
         painter->setPen(COLOR(Window).light(108));
         painter->drawEllipse(rect);
         // the value
         QFont fnt = painter->font();
         fnt.setPixelSize( rect.height()/3 );
         painter->setFont(fnt);
         painter->setBrush(Qt::NoBrush);
         painter->setPen(PAL.foreground().color());
         drawItemText(painter, rect,  Qt::AlignCenter, PAL, isEnabled,
                      QString::number(dial->sliderValue));
         // the drop
         painter->setPen(Qt::NoPen);
         rect = QRect(0,0,d,d);
         rect.moveCenter(cp);
         painter->setBrush(QColor(0,0,0,50));
         painter->drawEllipse(rect);
         rect.adjust(dpi.$2,dpi.$1,-dpi.$2,-dpi.$2);
         painter->setBrushOrigin(rect.topLeft());
         painter->setBrush(gradient(btnBgColor(PAL, isEnabled, hover||hasFocus),
                           rect.height(), Qt::Vertical, GradRadialGloss));
         painter->drawEllipse(rect);
         painter->restore();
      }
      break;
   default:
      QCommonStyle::drawComplexControl( control, option, painter, widget );
   } // switch
}
