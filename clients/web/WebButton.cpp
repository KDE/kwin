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
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/

#include <qpainter.h>

#include "WebButton.h"

using namespace KWinInternal;

WebButton::WebButton(QWidget * parent, const QString& tip)
  : KWinWidgetButton (parent, 0, 0, tip),
    mouseOver_  (false),
    mouseDown_  (false),
    position_   (Mid),
    shape_      (false)
{
  setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum));
  setBackgroundMode(NoBackground);
}

WebButton::~WebButton()
{
  // Empty.
}

  void
WebButton::setShape(bool b)
{
  shape_ = b;
  repaint();
}

  void
WebButton::mousePressEvent(QMouseEvent * e)
{
  mouseDown_ = true;
  repaint();
  KWinWidgetButton::mousePressEvent(e);
}

  void
WebButton::mouseReleaseEvent(QMouseEvent * e)
{
  mouseDown_ = false;
  repaint();

  if (rect().contains(e->pos()))
  {
    clickEvent(e->button());
  }
  KWinWidgetButton::mouseReleaseEvent(e);
}

  void
WebButton::enterEvent(QEvent * e)
{
  mouseOver_ = true;
  repaint();
  KWinWidgetButton::enterEvent(e);
}

  void
WebButton::leaveEvent(QEvent * e)
{
  mouseOver_ = false;
  repaint();
  KWinWidgetButton::leaveEvent(e);
}

  void
WebButton::paintEvent(QPaintEvent *)
{
  QPen highlightPen;

  if (mouseDown_)
    highlightPen = QPen(colorGroup().light());

  else
  {
    if (mouseOver_)
      highlightPen = QPen(colorGroup().highlight());
    else
      highlightPen = QPen(NoPen);
  }

  QPainter p(this);

  p.fillRect(rect(), colorGroup().background());

  switch (position_)
  {
    case Left:
      {
        // Draw edge.

        p.setPen(Qt::black);

        p.drawLine(0, 0, width(), 0);
        p.drawLine(0, 1, 0, height() - 1);

        if (shape_)
        {
          p.drawPoint(3, 1);
          p.drawPoint(4, 1);
          p.drawPoint(2, 2);
          p.drawPoint(1, 3);
          p.drawPoint(1, 4);
        }

        // Draw highlight.

        p.setBrush(NoBrush);
        p.setPen(highlightPen);

        if (shape_)
          p.setClipRegion(QRegion(rect()) - QRect(0, 0, 6, 6));

        p.drawRect(2, 2, width() - 4, height() - 4);

        if (shape_)
        {
          p.setClipRect(rect());
          p.drawPoint(4, 3);
          p.drawPoint(5, 3);
          p.drawPoint(3, 4);
          p.drawPoint(3, 5);
        }
      }

      break;

    case Right:
      {
        // Draw edge.

        p.setPen(Qt::black);
        p.drawLine(0, 0, width(), 0);
        p.drawLine(width() - 1, 1, width() - 1, height() - 1);

        if (shape_)
        {
          p.drawPoint(width() - 5, 1);
          p.drawPoint(width() - 4, 1);
          p.drawPoint(width() - 3, 2);
          p.drawPoint(width() - 2, 3);
          p.drawPoint(width() - 2, 4);
        }

        // Draw highlight.

        p.setBrush(NoBrush);
        p.setPen(highlightPen);

        if (shape_)
          p.setClipRegion(QRegion(rect()) - QRect(width() - 6, 0, 6, 6));

        p.drawRect(2, 2, width() - 4, height() - 4);

        if (shape_)
        {
          p.setClipRect(rect());
          p.drawPoint(width() - 5, 3);
          p.drawPoint(width() - 6, 3);
          p.drawPoint(width() - 4, 4);
          p.drawPoint(width() - 4, 5);
        }
      }

      break;

    case Mid:
    default:
      {
        // Draw edge.

        p.setPen(Qt::black);
        p.drawLine(0, 0, width(), 0);

        // Draw highlight.

        p.setBrush(NoBrush);
        p.setPen(highlightPen);

        p.drawRect(2, 2, width() - 4, height() - 4);
      }

      break;
  }

  // Draw icon.

  QPoint center(rect().center());

  int bwby2(bitmap_.width() / 2);    // Bitmap Width BY 2
  int bhby2(bitmap_.height() / 2);   // Bitmap Height BY 2

  p.setBrush(NoBrush);
  p.setPen(Qt::black);

  p.drawPixmap(center.x() - bwby2 + 1, center.y() - bhby2 + 1, bitmap_);
}

  QSize
WebButton::sizeHint() const
{
  return QSize(16, 16);
}

  QSize
WebButton::minimumSizeHint() const
{
  return QSize(14, 14);
}

  void
WebButton::setBitmap(const QBitmap & b)
{
  bitmap_ = b;
  repaint();
}

  void
WebButton::setPosition(Position p)
{
  position_ = p;
  repaint();
}

  void
WebButton::resizeEvent(QResizeEvent *)
{
  repaint();
}

#include "WebButton.moc"
// vim:ts=2:sw=2:tw=78:set et:
