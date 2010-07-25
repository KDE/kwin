/********************************************************************
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef __KWMTHEMECLIENT_H
#define __KWMTHEMECLIENT_H

#include <qbutton.h>
#include <QToolButton>
#include <QPixmap>
#include <kdecoration.h>
#include <kdecorationfactory.h>

class QSpacerItem;
class QGridLayout;

namespace KWMTheme {

class MyButton : public QToolButton
{
public:
    explicit MyButton(QWidget *parent=0, const char *name=0)
        : QToolButton(parent, name){setAutoRaise(true);setCursor( arrowCursor ); }
protected:
    void drawButtonLabel(QPainter *p);
};

class KWMThemeClient : public KDecoration
{
    Q_OBJECT
public:
    KWMThemeClient( KDecorationBridge* b, KDecorationFactory* f );
    ~KWMThemeClient(){;}
    void init();
    void resize( const QSize& s );
    QSize minimumSize() const;
    void borders( int& left, int& right, int& top, int& bottom ) const;
protected:
    void doShape();
    void drawTitle(QPainter &p);
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
    void showEvent( QShowEvent* );
    void mouseDoubleClickEvent( QMouseEvent * );
    bool eventFilter( QObject* o, QEvent* e );
    void captionChange();
    void desktopChange();
    void maximizeChange();
    void iconChange();
    void activeChange();
    void shadeChange() {};
    Position mousePosition(const QPoint &) const;
protected slots:
    //void slotReset();
    void menuButtonPressed();
    void slotMaximize();
private:
    QPixmap buffer;
    KPixmap *aGradient, *iGradient;
    MyButton *maxBtn, *stickyBtn, *mnuBtn;
    QSpacerItem *titlebar;
    QGridLayout* layout;
};

class KWMThemeFactory : public KDecorationFactory
{
public:
    KWMThemeFactory();
    ~KWMThemeFactory();
    KDecoration* createDecoration( KDecorationBridge* b );
    bool reset( unsigned long mask );
};

}

#endif

