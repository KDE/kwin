#ifndef __B2CLIENT_H
#define __B2CLIENT_H

#include <qtoolbutton.h>
#include <qbitmap.h>
#include <kpixmap.h>
#include "../../client.h"
class QLabel;
//class QSpacerItem;
//class QHBoxLayout;
class QGridLayout;

class B2Button : public QButton
{
public:
    B2Button(QWidget *parent=0, const char *name=0)
        : QButton(parent, name){useMiniIcon = false;}
    B2Button(KPixmap *pix, KPixmap *pixDown, QWidget *parent=0, const char *name=0);
    void setBg(const QColor &c){bg = c; setBackgroundColor(c);}
    void setPixmaps(KPixmap *pix, KPixmap *pixDown);
    void setToggle(){setToggleType(Toggle);}
    void setActive(bool on){setOn(on);}
    void setUseMiniIcon(){useMiniIcon = true;}
    QSize sizeHint() const;
    QSizePolicy sizePolicy() const;
protected:
    virtual void drawButton(QPainter *p);
    void drawButtonLabel(QPainter *){;}
    bool useMiniIcon;
    KPixmap *pNorm, *pDown;
    QColor bg; //only use one color (the rest is pixmap) so forget QPalette ;)
};

class B2Client : public Client
{
    Q_OBJECT
public:
    B2Client( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~B2Client(){;}
protected:
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
    void showEvent( QShowEvent* );
    void windowWrapperShowEvent( QShowEvent* );
    void mouseDoubleClickEvent( QMouseEvent * );
    void mousePressEvent( QMouseEvent * );
    void mouseReleaseEvent( QMouseEvent * );
    void mouseMoveEvent(QMouseEvent *);
    void init();
    void captionChange( const QString& name );
    void stickyChange(bool on);
    void activeChange(bool on);
    void maximizeChange(bool m);
    void doShape();
    MousePosition mousePosition( const QPoint& p ) const;
private slots:
    void menuButtonPressed();
    void slotReset();
private:
    void positionButtons();
    enum ButtonType{BtnMenu=0, BtnSticky, BtnIconify, BtnMax, BtnClose,
        BtnHelp};
    //B2Button* button[5];
    B2Button* button[6];
    //QSpacerItem* titlebar;
    //QHBoxLayout *tLayout;
    QGridLayout *g;
    int bar_x_ofs;
    bool shift_move;
    QPoint moveOffset;
};                      




#endif
