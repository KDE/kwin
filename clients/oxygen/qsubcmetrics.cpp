/***************************************************************************
 *   Copyright (C) 2006-2007 by Thomas Lübking                             *
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

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QTabBar>
#include <QStyleOption>
#include <QStyleOptionTab>
#include <limits.h>
#include "oxygen.h"

using namespace Oxygen;
extern Dpi dpi;

QRect OxygenStyle::subControlRect ( ComplexControl control, const QStyleOptionComplex * option, SubControl subControl, const QWidget * widget) const
{
   QRect ret;
   switch (control) {
   case CC_SpinBox: // A spinbox, like QSpinBox
      if (const QStyleOptionSpinBox *spinbox =
          qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
         QSize bs = spinbox->rect.size();
         bs.setHeight(bs.height()/2);
            // 1.6 -approximate golden mean
         bs.setWidth(qMax(dpi.$18, qMin(bs.height(), bs.width() / 4)));
//          bs = bs.expandedTo(QApplication::globalStrut());
         int x = spinbox->rect.width() - bs.width();
         switch (subControl) {
         case SC_SpinBoxUp:
            ret = QRect(x, 0, bs.width(), bs.height());
            break;
         case SC_SpinBoxDown:
            ret = QRect(x, bs.height(), bs.width(), spinbox->rect.height()-bs.height());
            break;
         case SC_SpinBoxEditField: {
            int hfw = 0, vfw = 0;
            if (spinbox->frame) {
               hfw = dpi.$3; vfw = dpi.$1;
            }
            // in next line -1 and +2 is too offset some qt weirdness so spinbox and linedit have same height
            ret = QRect(hfw, vfw-1, x-dpi.$1, spinbox->rect.height() - 2*vfw + 2);
            break;
         }
         case SC_SpinBoxFrame:
            ret = spinbox->rect;
         default:
            break;
         }
         ret = visualRect(spinbox->direction, spinbox->rect, ret);
      }
   case CC_ComboBox: // A combobox, like QComboBox
      if (const QStyleOptionComboBox *cb =
          qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
         int x,y,wi,he;
         cb->rect.getRect(&x,&y,&wi,&he);
         int margin = 1;//(!cb->editable) ? dpi.$1 : (cb->frame ? dpi.$4 : 0);

         switch (subControl) {
         case SC_ComboBoxFrame:
            ret = cb->rect;
            break;
         case SC_ComboBoxArrow:
            x += wi; wi = (int)((he - 2*margin)/1.1);//1.618
            x -= margin + wi; // golden mean
//             y += margin;
            ret.setRect(x, y, wi, he/* - 2*margin*/);
            break;
         case SC_ComboBoxEditField:
            wi -= (int)((he - 2*margin)/1.1) + 3*margin;
            ret.setRect(x+margin, y+margin, wi, he - 2*margin);
            break;
         case SC_ComboBoxListBoxPopup:
            ret = cb->rect;
            if (!cb->editable) { // shorten for the arrow
               wi -= (int)((he - 2*margin)/1.1) + 3*margin;
               ret.setRect(x+margin, y, wi, he);
            }
            break;
         default:
            break;
         }
         ret = visualRect(cb->direction, cb->rect, ret);
      }
      break;
   case CC_GroupBox:
      if (const QStyleOptionGroupBox *groupBox =
          qstyleoption_cast<const QStyleOptionGroupBox *>(option)) {
         switch (subControl) {
         case SC_GroupBoxFrame:
            ret = groupBox->rect;
            break;
         case SC_GroupBoxContents: {
            int top = dpi.$6;
            if (!groupBox->text.isEmpty())
               top += groupBox->fontMetrics.height();
            ret = groupBox->rect.adjusted(dpi.$3,top,-dpi.$3,-dpi.$7);
            break;
         }
         case SC_GroupBoxCheckBox: {
            int cbsz = pixelMetric(PM_IndicatorWidth, groupBox, widget);
            if (groupBox->direction == Qt::LeftToRight) {
               ret = groupBox->rect.adjusted(dpi.$5,dpi.$5,0,0);
               ret.setWidth(cbsz);
            }
            else {
               ret = groupBox->rect.adjusted(0,dpi.$5,-dpi.$5,0);
               ret.setLeft(ret.right()-cbsz);
            }
            ret.setHeight(cbsz);
            break;
         }
         case SC_GroupBoxLabel: {
            QFontMetrics fontMetrics = groupBox->fontMetrics;
            int h = fontMetrics.height()+dpi.$4;
            int tw = fontMetrics.size(Qt::TextShowMnemonic, groupBox->text + QLatin1Char(' ')).width();
            int marg = (groupBox->features & QStyleOptionFrameV2::Flat) ? 0 : dpi.$4;

            ret = groupBox->rect.adjusted(marg, dpi.$4, -marg, 0);
            ret.setHeight(h);

            // Adjusted rect for label + indicatorWidth + indicatorSpace
            ret = alignedRect(groupBox->direction, Qt::AlignHCenter, QSize(tw, h), ret);
            break;
         }
         default:
            break;
         }
      }
      break;
   case CC_ScrollBar: // A scroll bar, like QScrollBar
      if (const QStyleOptionSlider *scrollbar =
          qstyleoption_cast<const QStyleOptionSlider *>(option)) {
         int sbextent = pixelMetric(PM_ScrollBarExtent, scrollbar, widget);
         int maxlen = ((scrollbar->orientation == Qt::Horizontal) ?
                       scrollbar->rect.width() : scrollbar->rect.height()) - (sbextent * 2);
         int sliderlen;
         
            // calculate slider length
         if (scrollbar->maximum != scrollbar->minimum) {
            uint range = scrollbar->maximum - scrollbar->minimum;
            sliderlen = (qint64(scrollbar->pageStep) * maxlen) / (range + scrollbar->pageStep);
            
            int slidermin = pixelMetric(PM_ScrollBarSliderMin, scrollbar, widget);
            if (sliderlen < slidermin || range > INT_MAX / 2)
               sliderlen = slidermin;
            if (sliderlen > maxlen)
               sliderlen = maxlen;
         }
         else
            sliderlen = maxlen;
        
         int sliderstart = sliderPositionFromValue(scrollbar->minimum,
            scrollbar->maximum, scrollbar->sliderPosition, maxlen - sliderlen,
            scrollbar->upsideDown);
         switch (subControl) {
         case SC_ScrollBarSubLine:            // top/left button
            if (scrollbar->orientation == Qt::Horizontal) {
               int buttonWidth = qMin(scrollbar->rect.width() / 2, sbextent);
               ret.setRect(scrollbar->rect.width() - 2*buttonWidth, 0, buttonWidth, sbextent);
            }
            else {
               int buttonHeight = qMin(scrollbar->rect.height() / 2, sbextent);
               ret.setRect(0, scrollbar->rect.height() - 2*buttonHeight, sbextent, buttonHeight);
            }
            break;
         case SC_ScrollBarAddLine:            // bottom/right button
            if (scrollbar->orientation == Qt::Horizontal) {
               int buttonWidth = qMin(scrollbar->rect.width()/2, sbextent);
               ret.setRect(scrollbar->rect.width() - buttonWidth, 0, buttonWidth, sbextent);
            }
            else {
               int buttonHeight = qMin(scrollbar->rect.height()/2, sbextent);
               ret.setRect(0, scrollbar->rect.height() - buttonHeight, sbextent, buttonHeight);
            }
            break;
         case SC_ScrollBarSubPage:            // between top/left button and slider
            if (scrollbar->orientation == Qt::Horizontal)
               ret.setRect(dpi.$2, 0, sliderstart, sbextent);
            else
               ret.setRect(0, dpi.$2, sbextent, sliderstart);
            break;
         case SC_ScrollBarAddPage:            // between bottom/right button and slider
            if (scrollbar->orientation == Qt::Horizontal)
               ret.setRect(sliderstart + sliderlen - dpi.$2, 0,
                           maxlen - sliderstart - sliderlen, sbextent);
            else
               ret.setRect(0, sliderstart + sliderlen - dpi.$3,
                           sbextent, maxlen - sliderstart - sliderlen);
            break;
         case SC_ScrollBarGroove:
            if (scrollbar->orientation == Qt::Horizontal)
               ret.setRect(0, 0, scrollbar->rect.width() - sbextent * 2,
                           scrollbar->rect.height());
            else
               ret.setRect(0, 0, scrollbar->rect.width(),
                           scrollbar->rect.height() - sbextent * 2);
            break;
         case SC_ScrollBarSlider:
            if (scrollbar->orientation == Qt::Horizontal)
               ret.setRect(sliderstart, 0, sliderlen, sbextent);
            else
               ret.setRect(0, sliderstart, sbextent, sliderlen);
            break;
         default:
            break;
         }
         ret = visualRect(scrollbar->direction, scrollbar->rect, ret);
      }
      break;
//    case CC_Slider: // A slider, like QSlider
   case CC_ToolButton: // A tool button, like QToolButton
      if (const QStyleOptionToolButton *tb =
          qstyleoption_cast<const QStyleOptionToolButton *>(option)) {
         int mbi = pixelMetric(PM_MenuButtonIndicator, tb, widget);
         int fw = pixelMetric(PM_DefaultFrameWidth, tb, widget);
         QRect ret = tb->rect.adjusted(fw,fw,-fw,0);
         switch (subControl) {
         case SC_ToolButton:
            if ((tb->features
               & (QStyleOptionToolButton::Menu | QStyleOptionToolButton::PopupDelay))
               == QStyleOptionToolButton::Menu)
               ret.adjust(0, 0, -(mbi+fw), 0);
            break;
         case SC_ToolButtonMenu:
            if ((tb->features
               & (QStyleOptionToolButton::Menu | QStyleOptionToolButton::PopupDelay))
               == QStyleOptionToolButton::Menu)
               ret.adjust(ret.width() - mbi, 0, 0, 0);
            break;
         default:
            break;
         }
         return visualRect(tb->direction, tb->rect, ret);
        }
        break;
   case CC_TitleBar: // A Title bar, like what is used in Q3Workspace
      if (const QStyleOptionTitleBar *tb =
          qstyleoption_cast<const QStyleOptionTitleBar *>(option)) {
         const int controlMargin = 0;
         const int controlHeight = tb->rect.height()/* - controlMargin *2*/;
         const int delta = 8*controlHeight/5;
         int offset = 0;
         
         bool isMinimized = tb->titleBarState & Qt::WindowMinimized;
         bool isMaximized = tb->titleBarState & Qt::WindowMaximized;
         
         switch (subControl) {
         case SC_TitleBarLabel:
            if (tb->titleBarFlags &
                (Qt::WindowTitleHint | Qt::WindowSystemMenuHint)) {
               ret = tb->rect;
               ret.adjust(delta, 0, 0, 0);
               if (tb->titleBarFlags & Qt::WindowSystemMenuHint)
                  ret.adjust(delta, 0, -delta, 0);
               if (tb->titleBarFlags & Qt::WindowMinimizeButtonHint)
                  ret.adjust(delta, 0, 0, 0);
               if (tb->titleBarFlags & Qt::WindowMaximizeButtonHint)
                  ret.adjust(delta, 0, 0, 0);
               if (tb->titleBarFlags & Qt::WindowShadeButtonHint)
                  ret.adjust(0, 0, -delta, 0);
               if (tb->titleBarFlags & Qt::WindowContextHelpButtonHint)
                  ret.adjust(0, 0, -delta, 0);
            }
            break;
         case SC_TitleBarMaxButton:
            if (!isMaximized && (tb->titleBarFlags & Qt::WindowMaximizeButtonHint))
               offset += delta;
            else if (subControl == SC_TitleBarMaxButton)
               break;
         case SC_TitleBarMinButton:
            if (!isMinimized && (tb->titleBarFlags & Qt::WindowMinimizeButtonHint))
               offset += delta;
            else if (subControl == SC_TitleBarMinButton)
               break;
         case SC_TitleBarNormalButton:
            if (isMinimized && (tb->titleBarFlags & Qt::WindowMinimizeButtonHint))
               offset += delta;
            else if (isMaximized && (tb->titleBarFlags & Qt::WindowMaximizeButtonHint))
               offset += delta;
            else if (subControl == SC_TitleBarNormalButton)
               break;
         case SC_TitleBarCloseButton:
            if (tb->titleBarFlags & Qt::WindowSystemMenuHint)
               offset += controlMargin;
            else if (subControl == SC_TitleBarCloseButton)
               break;
            ret.setRect(tb->rect.left() + offset, tb->rect.top() + controlMargin, delta, controlHeight);
            break;
            
         case SC_TitleBarContextHelpButton:
            if (tb->titleBarFlags & Qt::WindowContextHelpButtonHint)
               offset += delta;
         case SC_TitleBarShadeButton:
            if (!isMinimized && (tb->titleBarFlags & Qt::WindowShadeButtonHint))
               offset += delta;
            else if (subControl == SC_TitleBarShadeButton)
               break;
         case SC_TitleBarUnshadeButton:
            if (isMinimized && (tb->titleBarFlags & Qt::WindowShadeButtonHint))
               offset += delta;
            else if (subControl == SC_TitleBarUnshadeButton)
               break;
         case SC_TitleBarSysMenu:
            if (tb->titleBarFlags & Qt::WindowSystemMenuHint)
               offset += delta + controlMargin;
            else if (subControl == SC_TitleBarSysMenu)
               break;
            ret.setRect(tb->rect.right() - offset, tb->rect.top() + controlMargin, controlHeight, controlHeight);
            break;
         default:
            break;
         }
         ret = visualRect(tb->direction, tb->rect, ret);
      }
      break;
   case CC_Q3ListView: // Used for drawing the Q3ListView class
   case CC_Dial: // A dial, like QDial
   default:
      ret = QCommonStyle::subControlRect ( control, option, subControl, widget);
   }
   return ret;
}

QRect OxygenStyle::subElementRect ( SubElement element, const QStyleOption * option, const QWidget * widget) const
{
   switch (element) {
   case SE_PushButtonContents: // Area containing the label (icon with text or pixmap)
      return visualRect(option->direction, option->rect,
                        option->rect.adjusted(dpi.$4,dpi.$4,-dpi.$4,-dpi.$4));
//    case SE_PushButtonFocusRect: // Area for the focus rect (usually larger than the contents rect)
//    case SE_CheckBoxIndicator: // Area for the state indicator (e.g., check mark)
//    case SE_CheckBoxContents: // Area for the label (text or pixmap)
//    case SE_CheckBoxFocusRect: // Area for the focus indicator
//    case SE_CheckBoxClickRect: // Clickable area, defaults to SE_CheckBoxFocusRect
//    case SE_RadioButtonIndicator: // Area for the state indicator
//    case SE_RadioButtonContents: // Area for the label
//    case SE_RadioButtonFocusRect: // Area for the focus indicator
//    case SE_RadioButtonClickRect: // Clickable area, defaults to SE_RadioButtonFocusRect
//    case SE_ComboBoxFocusRect: // Area for the focus indicator
//    case SE_SliderFocusRect: // Area for the focus indicator
//    case SE_Q3DockWindowHandleRect: // Area for the tear-off handle
   case SE_ProgressBarGroove: // Area for the groove
      return option->rect;//.adjusted(0,0,0,0);
   case SE_ProgressBarContents: // Area for the progress indicator
   case SE_ProgressBarLabel: // Area for the text label
      return option->rect.adjusted(dpi.$3,dpi.$4,-dpi.$3,-dpi.$5);
//    case SE_DialogButtonAccept: // Area for a dialog's accept button
//    case SE_DialogButtonReject: // Area for a dialog's reject button
//    case SE_DialogButtonApply: // Area for a dialog's apply button
//    case SE_DialogButtonHelp: // Area for a dialog's help button
//    case SE_DialogButtonAll: // Area for a dialog's all button
//    case SE_DialogButtonRetry: // Area for a dialog's retry button
//    case SE_DialogButtonAbort: // Area for a dialog's abort button
//    case SE_DialogButtonIgnore: // Area for a dialog's ignore button
//    case SE_DialogButtonCustom: // Area for a dialog's custom widget area (in the button row)
   case SE_HeaderArrow: { //
      int x,y,w,h;
      option->rect.getRect(&x,&y,&w,&h);
      int margin = dpi.$2;// ;) pixelMetric(QStyle::PM_HeaderMargin, opt, widget);
      QRect r;
      if (option->state & State_Horizontal)
         r.setRect(x + w - 2*margin - (h / 2), y + h/4 + margin, h / 2, h/2);
      else
         r.setRect(x + dpi.$5, y, h / 2, h / 2 - margin * 2);
      r = visualRect(option->direction, option->rect, r);
      return r;
   }
//    case SE_HeaderLabel: //  
//    case SE_TabWidgetLeftCorner: //  
//    case SE_TabWidgetRightCorner: //  
   case SE_TabWidgetTabBar: //  
      if (const QStyleOptionTabWidgetFrame *twf
          = qstyleoption_cast<const QStyleOptionTabWidgetFrame *>(option)) {
         QRect r(QPoint(0,0),twf->tabBarSize);
         int off = dpi.$10;
         switch (twf->shape) {
         case QTabBar::RoundedNorth:
         case QTabBar::TriangularNorth:
               // Constrain the size now, otherwise, center could get off the page
               // This of course repeated for all the other directions
               r.setWidth(qMin(r.width(), twf->rect.width() - 2*off
                                          - twf->leftCornerWidgetSize.width()
                                          - twf->rightCornerWidgetSize.width()));
               switch (styleHint(SH_TabBar_Alignment, twf, widget)) {
               default:
               case Qt::AlignLeft:
                  r.moveTopLeft(QPoint(twf->leftCornerWidgetSize.width()+off, 0));
                  break;
               case Qt::AlignCenter:
                  r.moveTopLeft(QPoint(twf->rect.center().x() - r.width() / 2, 0));
                  break;
               case Qt::AlignRight:
                  r.moveTopLeft(QPoint(twf->rect.width() - twf->tabBarSize.width() - off
                                       - twf->rightCornerWidgetSize.width(), 0));
                  break;
               }
               r = visualRect(twf->direction, twf->rect, r);
               break;
         case QTabBar::RoundedSouth:
         case QTabBar::TriangularSouth:
               r.setWidth(qMin(r.width(), twf->rect.width() - 2*off
                                          - twf->leftCornerWidgetSize.width()
                                          - twf->rightCornerWidgetSize.width()));
               switch (styleHint(SH_TabBar_Alignment, twf, widget)) {
               default:
               case Qt::AlignLeft:
                  r.moveTopLeft(QPoint(twf->leftCornerWidgetSize.width() + off,
                                       twf->rect.height() - twf->tabBarSize.height()));
                  break;
               case Qt::AlignCenter:
                  r.moveTopLeft(QPoint(twf->rect.center().x() - r.width() / 2,
                                       twf->rect.height() - twf->tabBarSize.height()));
                  break;
               case Qt::AlignRight:
                  r.moveTopLeft(QPoint(twf->rect.width() - twf->tabBarSize.width() - off
                                       - twf->rightCornerWidgetSize.width(),
                                       twf->rect.height() - twf->tabBarSize.height()));
                  break;
               }
               r = visualRect(twf->direction, twf->rect, r);
               break;
         case QTabBar::RoundedEast:
         case QTabBar::TriangularEast:
               r.setHeight(qMin(r.height(), twf->rect.height() - 2*off
                                          - twf->leftCornerWidgetSize.height()
                                          - twf->rightCornerWidgetSize.height()));
               switch (styleHint(SH_TabBar_Alignment, twf, widget)) {
               default:
               case Qt::AlignLeft:
                  r.moveTopLeft(QPoint(twf->rect.width() - twf->tabBarSize.width(),
                                       twf->leftCornerWidgetSize.height() + off));
                  break;
               case Qt::AlignCenter:
                  r.moveTopLeft(QPoint(twf->rect.width() - twf->tabBarSize.width(),
                                       twf->rect.center().y() - r.height() / 2));
                  break;
               case Qt::AlignRight:
                  r.moveTopLeft(QPoint(twf->rect.width() - twf->tabBarSize.width(),
                                       twf->rect.height() - twf->tabBarSize.height()
                                       - twf->rightCornerWidgetSize.height() - off));
                  break;
               }
               break;
         case QTabBar::RoundedWest:
         case QTabBar::TriangularWest:
               r.setHeight(qMin(r.height(), twf->rect.height() - 2*off)
                                          - twf->leftCornerWidgetSize.height()
                                          - twf->rightCornerWidgetSize.height());
               switch (styleHint(SH_TabBar_Alignment, twf, widget)) {
               default:
               case Qt::AlignLeft:
                  r.moveTopLeft(QPoint(0, twf->leftCornerWidgetSize.height() + off));
                  break;
               case Qt::AlignCenter:
                  r.moveTopLeft(QPoint(0, twf->rect.center().y() - r.height() / 2));
                  break;
               case Qt::AlignRight:
                  r.moveTopLeft(QPoint(0, twf->rect.height() - twf->tabBarSize.height()
                                       - twf->rightCornerWidgetSize.height() - off));
                  break;
               }
               break;
         }
         return r;
      }
   case SE_TabWidgetTabContents: //  
      if (const QStyleOptionTabWidgetFrame *twf =
          qstyleoption_cast<const QStyleOptionTabWidgetFrame *>(option)) {
         QRect r = option->rect; //subElementRect ( SE_TabWidgetTabPane, option, widget);
//          QStyleOptionTab tabopt;
//          tabopt.shape = twf->shape;
         const int margin = dpi.$4;
//          int baseHeight = pixelMetric(PM_TabBarBaseHeight, &tabopt, widget);
         switch (twf->shape) {
         case QTabBar::RoundedNorth:
         case QTabBar::TriangularNorth:
            r.adjust(margin, margin+twf->tabBarSize.height(), -margin, -margin);
            break;
         case QTabBar::RoundedSouth:
         case QTabBar::TriangularSouth:
            r.adjust(margin, margin, -margin, -margin-twf->tabBarSize.height());
            break;
         case QTabBar::RoundedEast:
         case QTabBar::TriangularEast:
            r.adjust(margin, margin, -margin-twf->tabBarSize.width(), -margin);
            break;
         case QTabBar::RoundedWest:
         case QTabBar::TriangularWest:
            r.adjust(margin+twf->tabBarSize.width(), margin, -margin, -margin);
         }
         return r;
      }
   case SE_TabWidgetTabPane: //  
      return option->rect;//.adjusted(-10,0,10,0);
   case SE_ToolBoxTabContents: // Area for a toolbox tab's icon and label
      return QRect(); //kill the rect, we paint the content ourself
//   case SE_ViewItemCheckIndicator: // Area for a view item's check mark
//    case SE_TabBarTearIndicator: // Area for the tear indicator on a tab bar with scroll arrows.
   default:
      return QCommonStyle::subElementRect ( element, option, widget);
   }
}
