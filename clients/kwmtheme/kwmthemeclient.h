#ifndef __KWMTHEMECLIENT_H
#define __KWMTHEMECLIENT_H

#include <qbutton.h>
#include <qtoolbutton.h>
#include <kpixmap.h>
#include "../../client.h"
class QLabel;
class QSpacerItem;

// QToolButton uses a 1 pixel border :P
class MyButton : public QToolButton
{
public:
    MyButton(QWidget *parent=0, const char *name=0)
        : QToolButton(parent, name){setAutoRaise(true);}
protected:
    void drawButtonLabel(QPainter *p);
};

class KWMThemeClient : public Client
{
    Q_OBJECT
public:
    KWMThemeClient( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~KWMThemeClient(){;}
protected:
    void doShape();
    void drawTitle(QPainter &p);
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
    void showEvent( QShowEvent* );
    void windowWrapperShowEvent( QShowEvent* );
    void mouseDoubleClickEvent( QMouseEvent * );
    void init();
    void captionChange( const QString& name );
    void stickyChange(bool on);
    void maximizeChange(bool m);
private:
    inline void tileHoriz(QPaintDevice *dest, const QPixmap *src,
                          int x, int y, int w);
    inline void tileVert(QPaintDevice *dest, const QPixmap *src,
                          int x, int y, int h);
    KPixmap *aGradient, *iGradient;
    MyButton *maxBtn, *stickyBtn;
    QSpacerItem *titlebar;
};

inline void KWMThemeClient::tileHoriz(QPaintDevice *dest, const QPixmap *src,
                                      int x, int y, int w)
{
    int cx;
    for(cx = x; cx < x+w; cx+=src->width()){
        bitBlt(dest, cx, y, src, 0, 0, cx+src->width() <= x+w ? src->width() :
               (x+w)-cx);
    }
}

inline void KWMThemeClient::tileVert(QPaintDevice *dest, const QPixmap *src,
                                     int x, int y, int h)
{
    int cy;
    for(cy = y; cy < y+h; cy+=src->height()){
        bitBlt(dest, x, cy, src, 0, 0, src->width(),
               cy+src->height() <= y+h ? src->height() :
               (y+h)-cy);
    }
}





#endif
