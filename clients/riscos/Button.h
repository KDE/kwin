/*
  RISC OS KWin client
  
  Copyright 2000
    Rik Hemsley <rik@kde.org>

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

#ifndef RISC_OS_BUTTON_H
#define RISC_OS_BUTTON_H

#include <qpixmap.h>
#include <qwidget.h>

namespace RiscOS
{

class Button : public QWidget
{
  Q_OBJECT

  public:

    enum Alignment { Left, Right };

    Button(QWidget * parent);
    virtual ~Button();

    void setAlignment(Alignment);
    void setActive(bool);

    Alignment alignment() const;

  protected:

    void paintEvent(QPaintEvent *);

    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);

    void setPixmap(const QPixmap &);

  private:

    Alignment   alignment_;
    bool        down_;
    bool        active_;
    QPixmap     pixmap_;
};

} // End namespace

#endif

// vim:ts=2:sw=2:tw=78
