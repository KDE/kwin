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
#include <qpixmap.h>
#include <qpoint.h>

#include "../../client.h"

class RiscOSClient;

class RiscOSButton : public QButton
{
  Q_OBJECT

  public:

    RiscOSButton(RiscOSClient * parent);

  protected:

    void drawButton(QPainter *);
    void setSymbol(const QPixmap &);

  protected:

    RiscOSClient * client() { return client_; }

  private:

    RiscOSClient * client_;

    QPixmap px_base_up_;
    QPixmap px_base_down_;
    
    QPixmap px_symbol_;
};

class RiscOSLowerButton : public RiscOSButton
{
  Q_OBJECT

  public:

    RiscOSLowerButton(RiscOSClient * parent);
};

class RiscOSCloseButton : public RiscOSButton
{
  Q_OBJECT

  public:

    RiscOSCloseButton(RiscOSClient * parent);
};

class RiscOSIconifyButton : public RiscOSButton
{
  Q_OBJECT

  public:

    RiscOSIconifyButton(RiscOSClient * parent);
};

class RiscOSMaximiseButton : public RiscOSButton
{
  Q_OBJECT

  public:

    RiscOSMaximiseButton(RiscOSClient * parent);

    void setOn(bool);

  protected:

    void mouseReleaseEvent(QMouseEvent *);
};

class RiscOSResizeButton : public RiscOSButton
{
  Q_OBJECT

  public:

    RiscOSResizeButton(RiscOSClient * parent);
};

class RiscOSTitleBar : public QWidget
{
  Q_OBJECT

  public:

    RiscOSTitleBar(RiscOSClient * parent);
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

    RiscOSClient * client_;

    QPixmap px_inactive_left_;
    QPixmap px_inactive_;
    QPixmap px_inactive_right_;
    QPixmap px_active_left_;
    QPixmap px_active_;
    QPixmap px_active_right_;

    QPixmap buf_;

    bool active_;

    QPoint clientPosToMousePos_;
};

class RiscOSResizeLeft;
class RiscOSResizeMid;
class RiscOSResizeRight;

class RiscOSResizeBar : public QWidget
{
  Q_OBJECT

  public:

    RiscOSResizeBar(RiscOSClient * parent);
    void update();

  private:

    RiscOSResizeLeft * left_;
    RiscOSResizeMid * mid_;
    RiscOSResizeRight * right_;
};

class RiscOSResizeMid : public QWidget
{
  Q_OBJECT

  public:

    RiscOSResizeMid(RiscOSResizeBar * parent, RiscOSClient * client);

    void update();

  protected:

    void resizeEvent(QResizeEvent *);
    void paintEvent(QPaintEvent *);
    void mouseMoveEvent(QMouseEvent *);

  private:

    RiscOSClient * client_;

    void _updatePixmap();

    QPixmap buf_;

    QPixmap px_left_;
    QPixmap px_mid_;
    QPixmap px_right_;
};

class RiscOSResizeLeft : public QWidget
{
  Q_OBJECT

  public:

    RiscOSResizeLeft(RiscOSResizeBar * parent, RiscOSClient * client);
  
  protected:

    void mouseMoveEvent(QMouseEvent *);
    
  private:
    
    RiscOSClient * client_;
};

class RiscOSResizeRight : public QWidget
{
  Q_OBJECT

  public:

    RiscOSResizeRight(RiscOSResizeBar * parent, RiscOSClient * client);

  protected:

    void mouseMoveEvent(QMouseEvent *);
    
  private:
    
    RiscOSClient * client_;
};

class RiscOSClient : public Client
{
  Q_OBJECT

  public:

    RiscOSClient(Workspace *, WId, QWidget * parent = 0, const char * name = 0);
    ~RiscOSClient() {}

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

    RiscOSLowerButton * lower_;
    RiscOSCloseButton * close_;

    RiscOSTitleBar * title_;

    RiscOSIconifyButton * iconify_;
    RiscOSMaximiseButton * maximize_;

    RiscOSResizeBar * resizeBar_;
};                      

#endif
// vim:ts=2:sw=2:tw=78
