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

#ifndef RISC_OS_CLIENT_H
#define RISC_OS_CLIENT_H

#include <qbutton.h>
#include <qpoint.h>

#include "../../client.h"

class QPixmap;

namespace RiscOS {

class Manager;

class Button : public QButton
{
  Q_OBJECT

  public:

    Button(Manager * parent);

    void update();

  protected:

    void drawButton(QPainter *);
    void setSymbols(QPixmap *, QPixmap *);

  protected:

    Manager * client() { return client_; }

  private:

    Manager * client_;
    
    QPixmap * px_symbol_inactive_;
    QPixmap * px_symbol_active_;
};

// --------------------------------------------------------------------------

class LowerButton : public Button
{
  Q_OBJECT

  public:

    LowerButton(Manager * parent);
};

// --------------------------------------------------------------------------

class CloseButton : public Button
{
  Q_OBJECT

  public:

    CloseButton(Manager * parent);
};

// --------------------------------------------------------------------------

class IconifyButton : public Button
{
  Q_OBJECT

  public:

    IconifyButton(Manager * parent);
};

// --------------------------------------------------------------------------

class MaximiseButton : public Button
{
  Q_OBJECT

  public:

    MaximiseButton(Manager * parent);

    void setOn(bool);

  protected:

    void mouseReleaseEvent(QMouseEvent *);
};

// --------------------------------------------------------------------------

class ResizeButton : public Button
{
  Q_OBJECT

  public:

    ResizeButton(Manager * parent);
};

// --------------------------------------------------------------------------

class TitleBar : public QWidget
{
  Q_OBJECT

  public:

    TitleBar(Manager * parent);
    virtual ~TitleBar();

    void update();

  protected:

    void paintEvent(QPaintEvent *);
    void resizeEvent(QResizeEvent *);

    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mouseDoubleClickEvent(QMouseEvent *);

  private:

    void _updatePixmap();

    Manager * client_;

    QPixmap * buf_;

    bool active_;

    QPoint clientPosToMousePos_;
};

// --------------------------------------------------------------------------

class ResizeBar;

class ResizeMid : public QWidget
{
  Q_OBJECT

  public:

    ResizeMid(ResizeBar * parent, Manager * client);
    virtual ~ResizeMid();

    void update();

  protected:

    void resizeEvent(QResizeEvent *);
    void paintEvent(QPaintEvent *);
    void mouseMoveEvent(QMouseEvent *);

  private:

    Manager * client_;

    void _updatePixmap();

    QPixmap * buf_;
};

// --------------------------------------------------------------------------

class ResizeLeft : public QWidget
{
  Q_OBJECT

  public:

    ResizeLeft(ResizeBar * parent, Manager * client);
  
  protected:

    void mouseMoveEvent(QMouseEvent *);
    
  private:
    
    Manager * client_;
};

// --------------------------------------------------------------------------

class ResizeRight : public QWidget
{
  Q_OBJECT

  public:

    ResizeRight(ResizeBar * parent, Manager * client);

  protected:

    void mouseMoveEvent(QMouseEvent *);
    
  private:
    
    Manager * client_;
};

// --------------------------------------------------------------------------

class ResizeBar : public QWidget
{
  Q_OBJECT

  public:

    ResizeBar(Manager * parent);
    void update();

  private:

    ResizeLeft * left_;
    ResizeMid * mid_;
    ResizeRight * right_;
};

// --------------------------------------------------------------------------

class Manager : public Client
{
  Q_OBJECT

  public:

    Manager(Workspace *, WId, QWidget * parent = 0, const char * name = 0);
    ~Manager() {}

    QColor colour() const { return options->color(Options::Font, isActive()); }
    QFont font() const { return options->font(isActive()); }

    void maximizeVertically();
    void maximizeAndRaise();
    void maximizeNoRaise();
    void resize(int, int);

    void setShade(bool);

  protected:

    void activeChange(bool);
    void maximizeChange(bool);
    void mousePressEvent(QMouseEvent *) {} // Disabled
    void mouseReleaseEvent(QMouseEvent *) {} // Disabled

  protected slots:

    void captionChange(const QString &);
    void slotReset();

  private:

    LowerButton * lower_;
    CloseButton * close_;

    TitleBar * title_;

    IconifyButton * iconify_;
    MaximiseButton * maximize_;

    ResizeBar * resizeBar_;

    void _loadPixmaps();
    bool pixmapsLoaded_;
};                      

} // End namespace `RiscOS'

#endif
// vim:ts=2:sw=2:tw=78
