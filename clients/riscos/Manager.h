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

#ifndef RISC_OS_MANAGER_H
#define RISC_OS_MANAGER_H

#include "../../client.h"

namespace RiscOS
{

class TitleBar;
class ResizeBar;

class Manager : public Client
{
  Q_OBJECT

  public:

    Manager(Workspace *, WId, QWidget * parent = 0, const char * name = 0);
    ~Manager();

    void maximizeVertically();
    void maximizeAndRaise();
    void maximizeNoRaise();
    void resize(int, int);

    void setShade(bool);

    void updateDisplay();

  protected:

    void paletteChange(const QPalette &);
    void activeChange(bool);
    void maximizeChange(bool);
    void paintEvent(QPaintEvent *);
    void mouseMoveEvent(QMouseEvent *) {} // Disabled
    void mousePressEvent(QMouseEvent *) {} // Disabled
    void mouseReleaseEvent(QMouseEvent *) {} // Disabled

  protected slots:

    void captionChange(const QString &);
    void slotReset();

  private:
   
    TitleBar * titleBar_;
    ResizeBar * resizeBar_;

};

} // End namespace

#endif

// vim:ts=2:sw=2:tw=78
