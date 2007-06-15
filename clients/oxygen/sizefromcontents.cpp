/***************************************************************************
 *   Copyright (C) 2006-2007 by Thomas L�bking  thomas.luebking@web.de
 *   Copyright (C) 2007 by Casper Boemann cbr@boemann.dk
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

#include <QStyleOptionComboBox>
#include <QStyleOptionMenuItem>
#include "oxygen.h"

#include <QtDebug>

using namespace Oxygen;
extern Dpi dpi;

extern Config config;
static const int windowsArrowHMargin = 6; // arrow horizontal margin

QSize OxygenStyle::sizeFromContents ( ContentsType ct, const QStyleOption * option, const QSize & contentsSize, const QWidget * widget ) const
{
   switch ( ct ) {
//    case CT_CheckBox: // A check box, like QCheckBox
   case CT_ComboBox: // A combo box, like QComboBox
      if (const QStyleOptionComboBox *cb =
          qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
         int hgt = contentsSize.height() - dpi.$1;
         if(!cb->editable)
           hgt += 2*dpi.$3;

         return QSize(contentsSize.width()+dpi.$10+(int)(hgt/1.1), hgt);
      }
//    case CT_DialogButtons: //
//       return QSize((contentsSize.width()+16 < 80) ? 80 : contentsSize.width()+16, contentsSize.height()+10);
//    case CT_Q3DockWindow: //  
   case CT_HeaderSection: // A header section, like QHeader
      if (const QStyleOptionHeader *hdr =
          qstyleoption_cast<const QStyleOptionHeader *>(option)) {
         QSize sz;
         int margin = dpi.$2;
         int iconSize = hdr->icon.isNull() ? 0 :
                pixelMetric(QStyle::PM_SmallIconSize, hdr, widget);
         QSize txt = hdr->fontMetrics.size(0, hdr->text);
         sz.setHeight(qMax(iconSize, txt.height()) + dpi.$4);
         sz.setWidth((iconSize?margin+iconSize:0) +
                     (hdr->text.isNull()?0:margin+txt.width()) +
                     ((hdr->sortIndicator == QStyleOptionHeader::None) ? 0 :
                      margin+8*option->rect.height()/5) + margin);
         return sz;
      }
   case CT_LineEdit: // A line edit, like QLineEdit
      return contentsSize ;//+ QSize(dpi.$2,dpi.$2);
   case CT_MenuBarItem: // A menu bar item, like the buttons in a QMenuBar
      return QSize(qMax(contentsSize.width()+dpi.$18, (contentsSize.height()+dpi.$8)*8/5), contentsSize.height()+dpi.$8);
   case CT_MenuItem: // A menu item, like QMenuItem
      if (const QStyleOptionMenuItem *menuItem =
          qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
         
         if (menuItem->menuItemType == QStyleOptionMenuItem::Separator)
            return QSize(10, menuItem->text.isEmpty() ?
                         dpi.$6 : menuItem->fontMetrics.lineSpacing());
             
         bool checkable = menuItem->menuHasCheckableItems;
         int maxpmw = config.showMenuIcons*menuItem->maxIconWidth;
         int w = contentsSize.width();
         int h = qMax(contentsSize.height()+dpi.$2,
                      menuItem->fontMetrics.lineSpacing());
         
         if (config.showMenuIcons && !menuItem->icon.isNull())
            h = qMax(h,
                     menuItem->icon.pixmap(pixelMetric(PM_SmallIconSize),
                                             QIcon::Normal).height() + dpi.$4);
         if (menuItem->text.contains('\t'))
            w += dpi.$12;
         if (maxpmw > 0)
            w += maxpmw + dpi.$6;
         if (checkable)
            w += 2*(h - dpi.$4)/3 + dpi.$7;
         w += (checkable + (maxpmw > 0))*dpi.$2;
         w += dpi.$12;
         if (menuItem->menuItemType == QStyleOptionMenuItem::SubMenu)
            w += 2 * windowsArrowHMargin;
         if (menuItem->menuItemType == QStyleOptionMenuItem::DefaultItem) {
            // adjust the font and add the difference in size.
            // it would be better if the font could be adjusted in the
            // getStyleOptions qmenu func!!
            QFontMetrics fm(menuItem->font);
            QFont fontBold = menuItem->font;
            fontBold.setBold(true);
            QFontMetrics fmBold(fontBold);
            w += fmBold.width(menuItem->text) - fm.width(menuItem->text);
         }
         return QSize(w, h);
      }
      break;
   case CT_PushButton: // A push button, like QPushButton
      if (const QStyleOptionButton *btn =
          qstyleoption_cast<const QStyleOptionButton *>(option)) {
         if (btn->text.isEmpty())
//            4 px for shadow & outline + 1px padding -> 5px per side
            return ( QSize( contentsSize.width() + dpi.$10, contentsSize.height() + dpi.$10 ) );
         else {
            QSize sz = contentsSize;
            if (btn->icon.isNull())
               return QSize((sz.width()+dpi.$20 < dpi.$80) ? dpi.$80 : contentsSize.width()+dpi.$20,
                         contentsSize.height()+dpi.$8);
            else
               return QSize((sz.width()+dpi.$20 < dpi.$80) ? dpi.$80 : contentsSize.width()+dpi.$20,
                         contentsSize.height()+dpi.$10);
         }
      }
//    case CT_RadioButton: // A radio button, like QRadioButton
//    case CT_SizeGrip: // A size grip, like QSizeGrip

   case CT_Menu: // A menu, like QMenu
   case CT_Q3Header: // A Qt 3 header section, like Q3Header
   case CT_MenuBar: // A menu bar, like QMenuBar
   case CT_ProgressBar: // A progress bar, like QProgressBar
   case CT_Slider: // A slider, like QSlider
   case CT_ScrollBar: // A scroll bar, like QScrollBar
   case CT_SpinBox: // A spin box, like QSpinBox
      return contentsSize;
//    case CT_Splitter: // A splitter, like QSplitter
   case CT_TabBarTab: // A tab on a tab bar, like QTabBar
      if (const QStyleOptionTab *tab =
          qstyleoption_cast<const QStyleOptionTab *>(option)) {
         switch (tab->shape) {
         case QTabBar::RoundedNorth: case QTabBar::TriangularNorth:
         case QTabBar::RoundedSouth: case QTabBar::TriangularSouth:
            return contentsSize + QSize(dpi.$8, 0);
         case QTabBar::RoundedEast: case QTabBar::TriangularEast:
         case QTabBar::RoundedWest: case QTabBar::TriangularWest:
            return contentsSize + QSize(0, dpi.$8);
         }
      }
      return contentsSize + QSize(dpi.$6, dpi.$6);
   case CT_TabWidget: // A tab widget, like QTabWidget
      return contentsSize + QSize(dpi.$8,dpi.$10);
   case CT_ToolButton: { // A tool button, like QToolButton
      const QStyleOptionToolButton *toolbutton
         = qstyleoption_cast<const QStyleOptionToolButton *>(option);
      // get ~goldem mean ratio
      int extraH = dpi.$8;
      if (toolbutton &&
          toolbutton->toolButtonStyle == Qt::ToolButtonTextUnderIcon)
         extraH = dpi.$10;
      int w = qMax(contentsSize.width()+dpi.$6, (contentsSize.height()+extraH)*7/5);
      if (toolbutton && (toolbutton->subControls & SC_ToolButtonMenu))
         w += pixelMetric(PM_MenuButtonIndicator, option, widget) + dpi.$8;
      return QSize(w, contentsSize.height()+extraH);
   }
   default: ;
   } // switch
   return QCommonStyle::sizeFromContents( ct, option, contentsSize, widget );
}
