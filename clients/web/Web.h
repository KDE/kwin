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

#include <qptrlist.h>

#include "../../lib/kdecoration.h"
#include "../../lib/kdecorationfactory.h"

class QLabel;
class QSpacerItem;
class QBoxLayout;

namespace Web
{

  class WebButton;

  class WebClient : public KDecoration
  {
    Q_OBJECT

    public:

      WebClient(KDecorationBridge* bridge, KDecorationFactory* factory);
      ~WebClient();

      virtual void init();
      virtual void resize(const QSize&);
      virtual bool eventFilter( QObject* o, QEvent* e );

    protected:
      virtual void reset( unsigned long changed );

      virtual void resizeEvent(QResizeEvent *);
      virtual void paintEvent(QPaintEvent *);
      virtual void showEvent(QShowEvent *);
      virtual void mouseDoubleClickEvent(QMouseEvent *);

      virtual void windowWrapperShowEvent(QShowEvent *);
      virtual void captionChange();
      virtual void desktopChange();
      virtual void maximizeChange();
      virtual void shadeChange() {};
      virtual void activeChange();
      virtual void iconChange();
      virtual void doShape();
      virtual MousePosition mousePosition(const QPoint &) const;
      virtual void borders(int&, int&, int&, int&) const;
      virtual QSize minimumSize() const;

    protected slots:

      void slotMaximize(int button);

    signals:

      void oadChange(bool);
      void maxChange(bool);

    private:
      bool isTool();

      enum ButtonType
      {
        ButtonHelp,
        ButtonOnAllDesktops,
        ButtonMenu,
        ButtonSeparator,
        ButtonIconify,
        ButtonMaximize,
        ButtonClose,
        ButtonLower
      };

      int titleHeight_, borderSize_;

      bool shape_;

      WebButton *   _createButton(const QString &, QWidget *  parent);
      void          _createButtons();
      void          _resetLayout();

      QBitmap       _buttonBitmap(ButtonType t) const;

      QBoxLayout    * mainLayout_;
      QSpacerItem   * titleSpacer_;

      QPtrList<WebButton> leftButtonList_;
      QPtrList<WebButton> rightButtonList_;
  };

  class WebFactory : public QObject, public KDecorationFactory
  {
    Q_OBJECT

    public:

      WebFactory() {};
      virtual ~WebFactory() {};
      virtual KDecoration* createDecoration( KDecorationBridge* );
      virtual bool reset( unsigned long changed );
      virtual QValueList< BorderSize > borderSizes() const;
  };
}

#endif
// vim:ts=2:sw=2:tw=78:set et:
