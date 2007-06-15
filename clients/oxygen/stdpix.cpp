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

#include <QStyleOptionTitleBar>
#include <QApplication>
#include <QPainter>
#include <QPen>
#include "oxygen.h"

#define COLOR(_TYPE_) pal.color(QPalette::_TYPE_)

using namespace Oxygen;

extern Dpi dpi;
extern Config config;

#include "inlinehelp.cpp"

QPixmap OxygenStyle::standardPixmap ( StandardPixmap standardPixmap, const QStyleOption * option, const QWidget * widget ) const
{

   QRect rect; QPalette pal; QPixmap pm;
   const QStyleOptionTitleBar *opt = qstyleoption_cast<const QStyleOptionTitleBar *>(option);
   if (opt) {
      pal = opt->palette;
      rect = opt->rect; rect.moveTo(0,0);
      pm = QPixmap(opt->rect.size());
   }
   else {
      rect = QRect(0,0,16,16);
      pal = qApp->palette();
      pm = QPixmap(rect.size());
   }
#if 0
   QString key =  QString::number(standardPixmap) + "@"
                  + QString::number(pm.width()) + "x" + QString::number(pm.height()) + ":"
                  + QString::number(midColor(pal.color(QPalette::WindowText), pal.color(QPalette::Window)).rgb());
   
   if (QPixmapCache::find(key, pm))
      return pm;
#endif
   pm.fill(Qt::transparent); // make transparent by default
   QPainter painter(&pm);
   bool sunken = false, isEnabled = false, hover = false;
   if (option) {
      sunken = option->state & State_Sunken;
      isEnabled = option->state & State_Enabled;
      hover = isEnabled && (option->state & State_MouseOver);
   }
   int sz = 2*qMin(rect.width(), rect.height())/3;
   QRect rndRect(0,0,sz,sz); bool foundRect = false;
   switch (standardPixmap) {
   case SP_DockWidgetCloseButton:
   case SP_TitleBarCloseButton:
      foundRect = true;
      rndRect.moveCenter(rect.center());
   case SP_TitleBarMinButton:
      if (!foundRect) {
         foundRect = true;
         rndRect.moveBottomLeft(rect.bottomLeft());
      }
   case SP_TitleBarNormalButton:
   case SP_TitleBarMaxButton:
      if (!foundRect)
         rndRect.moveTopRight(rect.topRight());
      painter.setRenderHint ( QPainter::Antialiasing );
      painter.setPen(Qt::NoPen);
      painter.setBrush(gradient(COLOR(Window), rect.height(), Qt::Vertical, GradSunken));
      painter.drawEllipse(rect);
      painter.setBrush(gradient(btnBgColor(pal, isEnabled, hover), rndRect.height(), Qt::Vertical, config.gradient));
      painter.setBrushOrigin(rndRect.topLeft());
      painter.drawEllipse(rndRect);
      break;
   case SP_TitleBarMenuButton: { //  0  Menu button on a title bar
      QFont fnt = painter.font();
      fnt.setPixelSize ( rect.height() );
      painter.setFont(fnt);
      painter.setPen(pal.color(QPalette::WindowText));
      painter.drawText(rect, Qt::AlignCenter, ";)");
      break;
   }
   case SP_TitleBarShadeButton: //  5  Shade button on title bars
      painter.drawPoint(rect.center().x(), rect.top());
      break;
   case SP_TitleBarUnshadeButton: //  6  Unshade button on title bars
      painter.drawPoint(rect.center().x(), rect.top());
      painter.drawPoint(rect.center());
      break;
   case SP_TitleBarContextHelpButton: { //  7  The Context help button on title bars
      QFont fnt = painter.font();
      fnt.setPixelSize ( rect.height() );
      painter.setFont(fnt);
      painter.drawText(rect, Qt::AlignCenter, "?");
      break;
   }
   case SP_MessageBoxInformation: { //  9  The "information" icon
      QFont fnt = painter.font(); fnt.setPixelSize ( rect.height()-2 ); painter.setFont(fnt);
      painter.setRenderHint ( QPainter::Antialiasing );
      painter.drawEllipse(rect);
      painter.drawText(rect, Qt::AlignHCenter | Qt::AlignBottom, "!");
      break;
   }
   case SP_MessageBoxWarning: { //  10  The "warning" icon
      QFont fnt = painter.font(); fnt.setPixelSize ( rect.height()-2 ); painter.setFont(fnt);
      painter.setRenderHint ( QPainter::Antialiasing );
      int hm = rect.x()+rect.width()/2;
      const QPoint points[4] = {
         QPoint(hm, rect.top()),
         QPoint(rect.left(), rect.bottom()),
         QPoint(rect.right(), rect.bottom()),
         QPoint(hm, rect.top())
      };
      painter.drawPolyline(points, 4);
      painter.drawLine(hm, rect.top()+4, hm, rect.bottom() - 6);
      painter.drawPoint(hm, rect.bottom() - 3);
      break;
   }
//    case SP_MessageBoxCritical: //  11  The "critical" icon
//       QFont fnt = painter.font(); fnt.setPixelSize ( rect.height() ); painter.setFont(fnt);
//       const QPoint points[3] =
//       {
//          QPoint(rect.width()/3, rect.top()),
//          QPoint(2*rect.width()/3, rect.top()),
//          QPoint(rect.right(), rect.height()/2),
//          QPoint(rect.right(), rect.bottom())
//       };
//       painter.drawPolyLine(points);
//       painter.drawText(rect, Qt::AlignCenter, "-");
   case SP_MessageBoxQuestion: { //  12  The "question" icon
      QFont fnt = painter.font(); fnt.setPixelSize ( rect.height() ); painter.setFont(fnt);
      painter.drawText(rect, Qt::AlignCenter, "?");
      break;
   }
//    case SP_DesktopIcon: //  13   
//    case SP_TrashIcon: //  14   
//    case SP_ComputerIcon: //  15   
//    case SP_DriveFDIcon: //  16   
//    case SP_DriveHDIcon: //  17   
//    case SP_DriveCDIcon: //  18   
//    case SP_DriveDVDIcon: //  19   
//    case SP_DriveNetIcon: //  20   
//    case SP_DirOpenIcon: //  21   
//    case SP_DirClosedIcon: //  22   
//    case SP_DirLinkIcon: //  23   
//    case SP_FileIcon: //  24   
//    case SP_FileLinkIcon: //  25   
//    case SP_FileDialogStart: //  28   
//    case SP_FileDialogEnd: //  29   
//    case SP_FileDialogToParent: //  30   
//    case SP_FileDialogNewFolder: //  31   
//    case SP_FileDialogDetailedView: //  32   
//    case SP_FileDialogInfoView: //  33   
//    case SP_FileDialogContentsView: //  34   
//    case SP_FileDialogListView: //  35   
//    case SP_FileDialogBack: //  36   
   ///    case SP_ToolBarHorizontalExtensionButton: //  26  Extension button for horizontal toolbars
   ///    case SP_ToolBarVerticalExtensionButton: //  27  Extension button for vertical toolbars
   default:
      return QCommonStyle::standardPixmap ( standardPixmap, option, widget );
   }
   painter.end();
#if 0
   QPixmapCache::insert(key, pm);
#endif
   return pm;
}

#undef COLOR
