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

#include <qdict.h>

#include "../../client.h"

class QSpacerItem;
class QVBoxLayout;
class QHBoxLayout;

namespace RiscOS
{

class LowerButton;
class CloseButton;
class IconifyButton;
class MaximiseButton;
class StickyButton;
class HelpButton;
class Button;

class Manager : public KWinInternal::Client
{
  Q_OBJECT

  public:

    Manager
      (
       KWinInternal::Workspace *,
       WId,
       QWidget    * parent  = 0,
       const char * name    = 0
      );

    ~Manager();

  signals:

    void maximiseChanged(bool);
    void stickyChanged(bool);
    void activeChanged(bool);

  public slots:

    void slotHelp();
    void slotLower();
    void slotRaise();
    void slotMax();
    void slotVMax();
    void slotSetSticky(bool);

  protected:

    KWinInternal::Client::MousePosition mousePosition(const QPoint &) const;
    void paletteChange(const QPalette &);
    void activeChange(bool);
    void maximizeChange(bool);
    void stickyChange(bool);
    void paintEvent(QPaintEvent *);
    void resizeEvent(QResizeEvent *);
    void mouseDoubleClickEvent(QMouseEvent *);
    void animateIconifyOrDeiconify(bool);
    void animate(bool, int);
    void updateButtonVisibility();
    void updateTitleBuffer();

    void createTitle();
    Button * createButton(int type, QWidget * parent);

    void resetLayout();

  protected slots:

    void captionChange(const QString &);
    void slotReset();

  private:

    QVBoxLayout     * topLayout_;
    QHBoxLayout     * titleLayout_;
    QSpacerItem     * titleSpacer_;

    QPixmap         titleBuf_;
    QList<Button>   leftButtonList_;
    QList<Button>   rightButtonList_;
};

class ToolManager : public Manager
{
  Q_OBJECT

  public:

    ToolManager(KWinInternal::Workspace *, WId, QWidget * parent = 0, const char * name = 0);
    ~ToolManager();
};


} // End namespace

#endif

// vim:ts=2:sw=2:tw=78
