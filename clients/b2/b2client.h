/*
 * B-II KWin Client
 *
 * Changes:
 * 	Customizable button positions by Karol Szwed <gallium@kde.org>
 * 	Ported to the kde3.2 API by Luciano Montanaro <mikelima@cirulla.net>
 */

#ifndef __B2CLIENT_H
#define __B2CLIENT_H

#include <qvariant.h>
#include <qdatetime.h>
#include <q3button.h>
#include <qbitmap.h>
//Added by qt3to4:
#include <QPaintEvent>
#include <QGridLayout>
#include <QEvent>
#include <QBoxLayout>
#include <QShowEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <kpixmap.h>
#include <kdecoration.h>
#include <kdecorationfactory.h>

class QSpacerItem;
class QBoxLayout;
class QGridLayout;

namespace B2 {

class B2Client;

class B2Button : public Q3Button
{
public:
    B2Button(B2Client *_client=0, QWidget *parent=0, const QString& tip=NULL, const int realizeBtns = Qt::LeftButton);
    ~B2Button() {};

    void setBg(const QColor &c){bg = c;}
    void setPixmaps(KPixmap *pix, KPixmap *pixDown, KPixmap *iPix,
                    KPixmap *iPixDown);
    void setPixmaps(int button_id);
    void setToggle(){setCheckable(true);}
    void setActive(bool on){setChecked(on);}
    void setUseMiniIcon(){useMiniIcon = true;}
    QSize sizeHint() const;
    QSizePolicy sizePolicy() const;
protected:
    virtual void drawButton(QPainter *p);
    void drawButtonLabel(QPainter *){;}

    void mousePressEvent( QMouseEvent* e );
    void mouseReleaseEvent( QMouseEvent* e );
private:
    void enterEvent(QEvent *e);
    void leaveEvent(QEvent *e);
    
    bool useMiniIcon;
    KPixmap *icon[6];
    QColor bg; //only use one color (the rest is pixmap) so forget QPalette ;)

public:
    B2Client* client;
    Qt::ButtonState last_button;
    int realizeButtons;
    bool hover;
};

class B2Titlebar : public QWidget
{
    friend class B2Client;
public:
    B2Titlebar(B2Client *parent);
    ~B2Titlebar(){;}
    bool isFullyObscured() const {return isfullyobscured;}
    void recalcBuffer();
    QSpacerItem *captionSpacer;
protected:
    void paintEvent( QPaintEvent* );
    bool x11Event(XEvent *e);
    void mouseDoubleClickEvent( QMouseEvent * );
    void mousePressEvent( QMouseEvent * );
    void mouseReleaseEvent( QMouseEvent * );
    void mouseMoveEvent(QMouseEvent *);
    void resizeEvent(QResizeEvent *ev);
private:
    void drawTitlebar(QPainter &p, bool state);

    B2Client *client;
    QString oldTitle;
    KPixmap titleBuffer;
    QPoint moveOffset;
    bool set_x11mask;
    bool isfullyobscured;
    bool shift_move;
};

class B2Client : public KDecoration
{
    Q_OBJECT
    friend class B2Titlebar;
public:
    B2Client(KDecorationBridge *b, KDecorationFactory *f);
    ~B2Client(){;}
    void init();
    void unobscureTitlebar();
    void titleMoveAbs(int new_ofs);
    void titleMoveRel(int xdiff);
    // transparent stuff
    virtual bool drawbound(const QRect& geom, bool clear);
protected:
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
    void showEvent( QShowEvent* );
    void windowWrapperShowEvent( QShowEvent* );
    void captionChange();
    void desktopChange();
    void shadeChange();
    void activeChange();
    void maximizeChange();
    void iconChange();
    void doShape();
    Position mousePosition( const QPoint& p ) const;
    void resize(const QSize&);
    void borders(int &, int &, int &, int &) const;
    QSize minimumSize() const;
    bool eventFilter(QObject *, QEvent *);
private slots:
    void menuButtonPressed();
    //void slotReset();
    void maxButtonClicked();
    void shadeButtonClicked();
    void resizeButtonPressed();
private:
    void addButtons(const QString& s, const QString tips[],
                    B2Titlebar* tb, QBoxLayout* titleLayout);
    void positionButtons();
    void calcHiddenButtons();
    bool mustDrawHandle() const;
    
    enum ButtonType{BtnMenu=0, BtnSticky, BtnIconify, BtnMax, BtnClose,
        BtnHelp, BtnShade, BtnResize, BtnCount};
    B2Button* button[BtnCount];
    QGridLayout *g;
    // Border spacers
    QSpacerItem *topSpacer; 
    QSpacerItem *bottomSpacer; 
    QSpacerItem *leftSpacer;
    QSpacerItem *rightSpacer;
    B2Titlebar *titlebar;
    int bar_x_ofs;
    int in_unobs;
    QTime time;
    bool resizable;
};

class B2ClientFactory : public QObject, public KDecorationFactory
{
public:
    B2ClientFactory();
    virtual ~B2ClientFactory();
    virtual KDecoration *createDecoration(KDecorationBridge *);
    virtual bool reset(unsigned long changed);
    virtual bool supports( Ability ability );
    QList< B2ClientFactory::BorderSize > borderSizes() const;
};

}

#endif
