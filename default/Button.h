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

#ifndef DEFAULT_BUTTON_H
#define DEFAULT_BUTTON_H

#include "Static.h"
#include <qwidget.h>

namespace Default
{

class Button : public QWidget
{
  Q_OBJECT

  public:

    enum Alignment { Left, Right };

    Button(QWidget * parent, SymbolType);
    virtual ~Button();
    
    void updateDisplay();

    void setAlign(Alignment a)  { align_ = a; updateDisplay(); }
    void setType(SymbolType t);

    void setActive(bool);
 
  protected:

    bool active() const { return active_; }

    void mousePressEvent(QMouseEvent *)   { down_ = true; updateDisplay(); }
    void mouseReleaseEvent(QMouseEvent *) { down_ = false; updateDisplay(); }

  private:

    SymbolType  type_;
    Alignment   align_;
    bool        down_;
    bool        active_;
};

} // End namespace

#endif

// vim:ts=2:sw=2:tw=78
