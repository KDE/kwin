/*
  'Web' kwin client

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
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#include "WebButton.h"
#include <QPainter>
//Added by qt3to4:
#include <QEvent>

#include "Web.h"

namespace Web {

  static unsigned char close_bits[] = {
    0x42, 0xe7, 0x7e, 0x3c, 0x3c, 0x7e, 0xe7, 0x42
  };
  static unsigned char iconify_bits[] = {
    0x00, 0x00, 0x00, 0x7e, 0x7e, 0x3c, 0x18, 0x00
  };
  static unsigned char maximize_bits[] = {
    0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x00, 0x00
  };
  static unsigned char unmaximize_bits[] = {
    0x00, 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f
  };
  static unsigned char sticky_bits[] = {
    0x20, 0x70, 0xfa, 0x7e, 0x3c, 0x1c, 0x32, 0x01
  };
  static unsigned char unsticky_bits[] = {
    0x1c, 0x1c, 0x1c, 0x3e, 0x7f, 0x08, 0x08, 0x08
  };
  static unsigned char help_bits[] = {
    0x18, 0x18, 0x00, 0x1c, 0x18, 0x18, 0x18, 0x3c
  };
  static unsigned char shade_on_bits[] = {
    0xff, 0xff, 0x81, 0x81, 0x99, 0xbd, 0x81, 0xff
  };
  static unsigned char shade_off_bits[] = {
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  static unsigned char above_on_bits[] = {
    0xff, 0x7e, 0x3c, 0x18, 0x00, 0xff, 0xff, 0x00
  };
  static unsigned char above_off_bits[] = {
    0x18, 0x3c, 0x7e, 0xff, 0x00, 0xff, 0xff, 0x00
  };
  static unsigned char below_on_bits[] = {
    0x00, 0xff, 0xff, 0x00, 0x18, 0x3c, 0x7e, 0xff
  };
  static unsigned char below_off_bits[] = {
    0x00, 0xff, 0xff, 0x00, 0xff, 0x7e, 0x3c, 0x18
  };
  static unsigned char menu_bits[] = {
    0xff, 0x81, 0x81, 0xff, 0x81, 0xff, 0x81, 0xff
  };

WebButton::WebButton(ButtonType type, WebClient *parent, bool shape)
  : KCommonDecorationButton (type, parent),
    mouseOver_  (false),
    shape_      (shape),
    deco_       (parent)
{
    setAttribute(Qt::WA_NoSystemBackground, true);
}

WebButton::~WebButton()
{
  // Empty.
}

void WebButton::reset(unsigned long changed)
{
  if (changed&DecorationReset || changed&ManualReset || changed&SizeChange || changed&StateChange) {
    switch (type() ) {
      case CloseButton:
        setBitmap(close_bits);
        break;
      case HelpButton:
        setBitmap(help_bits);
        break;
      case MinButton:
        setBitmap(iconify_bits);
        break;
      case MaxButton:
        setBitmap( isChecked() ? unmaximize_bits : maximize_bits );
        break;
      case OnAllDesktopsButton:
        setBitmap( isChecked() ? unsticky_bits : sticky_bits );
        break;
      case ShadeButton:
        setBitmap( isChecked() ? shade_on_bits : shade_off_bits );
        break;
      case AboveButton:
        setBitmap( isChecked() ? above_on_bits : above_off_bits );
        break;
      case BelowButton:
        setBitmap( isChecked() ? below_on_bits : below_off_bits );
        break;
      case MenuButton:
        setBitmap(menu_bits);
        break;
      default:
        setBitmap(0);
        break;
    }

    this->update();
  }
}

  void
WebButton::enterEvent(QEvent * e)
{
  mouseOver_ = true;
  repaint();
  QAbstractButton::enterEvent(e);
}

  void
WebButton::leaveEvent(QEvent * e)
{
  mouseOver_ = false;
  repaint();
  QAbstractButton::leaveEvent(e);
}

void WebButton::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  drawButton(&p);
}

  void
WebButton::drawButton(QPainter *p)
{
  QPen highlightPen;

  if (isDown() )
      highlightPen = QPen( palette().color( QPalette::Light ));

  else
  {
    if (mouseOver_)
        highlightPen = QPen( palette().color( QPalette::Highlight ));
    else
        highlightPen = QPen(Qt::NoPen);
  }

  p->fillRect(rect(), palette().color( QPalette::Background ) );

  Position position_;
  if (0 == mapToParent(rect().topLeft() ).x() )
    position_ = Left;
  else if (deco_->width()-1 == mapToParent(rect().topRight() ).x() )
    position_ = Right;
  else
    position_ = Mid;
  
  if( deco_->isPreview() && shape_ ) 
  {
    QRegion mask( deco_->calcMask().translated( -mapToParent(rect().topLeft()) ) );
    p->setClipRegion( mask, Qt::IntersectClip );
  }
  
  switch ( position_ )
  {
    case Left:
      {
        
        // Draw edge.
        p->setPen(Qt::black);

        p->drawLine(0, 0, width(), 0);
        p->drawLine(0, 1, 0, height() - 1);
        if (shape_)
        {
          p->drawPoint(3, 1);
          p->drawPoint(4, 1);
          p->drawPoint(2, 2);
          p->drawPoint(1, 3);
          p->drawPoint(1, 4);
        }
        // Draw highlight.

        p->setBrush(Qt::NoBrush);
        p->setPen(highlightPen);

        if (shape_)
          p->setClipRegion(QRegion(rect()) - QRect(0, 0, 6, 6));

        p->drawRect(2, 2, width() - 4, height() - 4);
        if (shape_)
        {
          p->setClipRect(rect());
          p->drawPoint(4, 3);
          p->drawPoint(5, 3);
          p->drawPoint(3, 4);
          p->drawPoint(3, 5);
        }
      }

      break;

    case Right:
      {
        // Draw edge.
        p->setPen(Qt::black);
        p->drawLine(0, 0, width(), 0);
        p->drawLine(width() - 1, 1, width() - 1, height() - 1);
        if (shape_)
        {
          p->drawPoint(width() - 5, 1);
          p->drawPoint(width() - 4, 1);
          p->drawPoint(width() - 3, 2);
          p->drawPoint(width() - 2, 3);
          p->drawPoint(width() - 2, 4);
        }
        // Draw highlight.

        p->setBrush(Qt::NoBrush);
        p->setPen(highlightPen);

        if (shape_)
          p->setClipRegion(QRegion(rect()) - QRect(width() - 6, 0, 6, 6));

        p->drawRect(2, 2, width() - 4, height() - 4);
        if (shape_)
        {
          p->setClipRect(rect());
          p->drawPoint(width() - 5, 3);
          p->drawPoint(width() - 6, 3);
          p->drawPoint(width() - 4, 4);
          p->drawPoint(width() - 4, 5);
        }
      }

      break;

    case Mid:
    default:
      {
        // Draw edge.

        p->setPen(Qt::black);
        p->drawLine(0, 0, width(), 0);

        // Draw highlight.

        p->setBrush(Qt::NoBrush);
        p->setPen(highlightPen);

        p->drawRect(2, 2, width() - 4, height() - 4);
      }

      break;
  }

  // Draw icon.

  QPoint center(rect().center());

  QPainterPath path;
  path.addRegion( bitmap_ );
  int bwby2(bitmap_.width() / 2);    // Bitmap Width BY 2
  int bhby2(bitmap_.height() / 2);   // Bitmap Height BY 2
  p->setPen( Qt::NoPen );
  p->setBrush(Qt::black);
  p->translate( center.x() - bwby2 + 1, center.y() - bhby2 + 1 );
  p->drawPath( path );
  
}

  void
WebButton::setBitmap(const unsigned char *bitmap)
{
  if (bitmap)
    bitmap_ = QBitmap::fromData( QSize(8, 8),  bitmap);
  else
    bitmap_ = QBitmap(8,8);
  bitmap_.setMask(bitmap_);
}

}

// vim:ts=2:sw=2:tw=78:set et:
// kate: indent-width 2; replace-tabs on; tab-width 2; space-indent on;
