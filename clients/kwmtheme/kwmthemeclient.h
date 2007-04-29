#ifndef __KWMTHEMECLIENT_H
#define __KWMTHEMECLIENT_H

#include <qbutton.h>
#include <QToolButton>
#include <QPixmap>
#include <kdecoration.h>
#include <kdecorationfactory.h>

class QLabel;
class QSpacerItem;
class QGridLayout;

namespace KWMTheme {

class MyButton : public QToolButton
{
public:
    MyButton(QWidget *parent=0, const char *name=0)
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

