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

#ifndef KWIN_WEB_H
#define KWIN_WEB_H

#include <qlist.h>

#include <kwin/client.h>

class QLabel;
class QSpacerItem;
class QBoxLayout;

namespace KWinInternal
{
  class WebButton;

  class Web : public KWinInternal::Client
  {
    Q_OBJECT

    public:

      Web
        (
         Workspace *,
         WId,
         bool tool,
         QWidget * parent = 0,
         const char * name = 0
        );

      ~Web();

    protected:

      virtual void resizeEvent(QResizeEvent *);
      virtual void paintEvent(QPaintEvent *);
      virtual void showEvent(QShowEvent *);
      virtual void mouseDoubleClickEvent(QMouseEvent *);

      virtual void windowWrapperShowEvent(QShowEvent *);
      virtual void captionChange(const QString &);
      virtual void stickyChange(bool on);
      virtual void maximizeChange(bool);
      virtual void doShape();
      virtual void activeChange(bool);
      virtual MousePosition mousePosition(const QPoint &) const;
      virtual void animateIconifyOrDeiconify(bool);

    protected slots:

      void slotReset();
      void slotMaximize(int button);

    signals:

      void stkyChange(bool);
      void maxChange(bool);

    private:

      enum ButtonType
      {
        ButtonHelp,
        ButtonSticky,
        ButtonMenu,
        ButtonSeparator,
        ButtonIconify,
        ButtonMaximize,
        ButtonClose,
        ButtonLower
      };

      bool shape_;
      bool tool_;

      WebButton *   _createButton(const QString &, QWidget *  parent);
      void          _createButtons();
      void          _resetLayout();

      QBitmap       _buttonBitmap(ButtonType t) const;

      QBoxLayout    * mainLayout_;
      QSpacerItem   * titleSpacer_;

      QList<WebButton> leftButtonList_;
      QList<WebButton> rightButtonList_;
  };
}

#endif
// vim:ts=2:sw=2:tw=78:set et:
