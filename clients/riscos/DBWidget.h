/*
  Default KWin client
  
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

#ifndef DEFAULT_DOUBLE_BUFFERED_WIDGET_H
#define DEFAULT_DOUBLE_BUFFERED_WIDGET_H

#include <qwidget.h>
#include <qpixmap.h>

namespace Default
{

class DBWidget : public QWidget
{
  public:

    DBWidget(QWidget * parent = 0, const char * name = 0);
    virtual ~DBWidget() { /* Empty */ }

    virtual void updateDisplay();

  protected:
    
    virtual void updatePixmap() = 0;

    virtual void paintEvent(QPaintEvent * e);
    virtual void resizeEvent(QResizeEvent * e);

    QPixmap & buf() { return buf_; }


  private:

    DBWidget(const DBWidget &);
    DBWidget & operator = (const DBWidget &);

    QPixmap buf_;
};

} // End namespace

#endif

// vim:ts=2:sw=2:tw=78
