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
    B2Button(Client *_client=0, QWidget *parent=0, const char *name=0)
        : QButton(parent, name){useMiniIcon = false; client = _client;}
    B2Button(KPixmap *pix, KPixmap *pixDown, KPixmap *iPix, KPixmap *iPixDown,
             Client *_client=0, QWidget *parent=0, const char *name=0);
    void setBg(const QColor &c){bg = c;}
    void setPixmaps(KPixmap *pix, KPixmap *pixDown, KPixmap *iPix,
                    KPixmap *iPixDown);
    void setToggle(){setToggleType(Toggle);}
    void setActive(bool on){setOn(on);}
    void setUseMiniIcon(){useMiniIcon = true;}
    QSize sizeHint() const;
    QSizePolicy sizePolicy() const;
protected:
    virtual void drawButton(QPainter *p);
    void drawButtonLabel(QPainter *){;}
    bool useMiniIcon;
    KPixmap *pNorm, *pDown, *iNorm, *iDown;
    QColor bg; //only use one color (the rest is pixmap) so forget QPalette ;)
    Client *client;
};

class B2Client;
class B2Titlebar : public QWidget
{
    Q_OBJECT
public:
    B2Titlebar(B2Client *parent);
    ~B2Titlebar(){;}
    bool isFullyObscured() const {return isfullyobscured;}
protected:
    void paintEvent( QPaintEvent* );
    bool x11Event(XEvent *e);
    void mouseDoubleClickEvent( QMouseEvent * );
    void mousePressEvent( QMouseEvent * );
    void mouseReleaseEvent( QMouseEvent * );
    void mouseMoveEvent(QMouseEvent *);
    void init();
    bool set_x11mask;
    bool isfullyobscured;
    bool shift_move;
    QPoint moveOffset;
    B2Client *client;
};

class B2Client : public Client
{
    Q_OBJECT
    friend class B2Titlebar;
public:
    B2Client( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~B2Client(){;}
    void unobscureTitlebar();
    void titleMoveAbs(int new_ofs);
    void titleMoveRel(int xdiff);
protected:
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
    void showEvent( QShowEvent* );
    void windowWrapperShowEvent( QShowEvent* );
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
    B2Titlebar *titlebar;
    int in_unobs;
};




#endif
