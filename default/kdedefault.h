#ifndef __KDECLIENT_H
#define __KDECLIENT_H

#include <qtoolbutton.h>
#include <qbitmap.h>
#include <kpixmap.h>
#include "../client.h"
class QLabel;
class QSpacerItem;
class QHBoxLayout;


// get rid of autohide :P
class KDEDefaultClientButton : public QToolButton
{
public:
    KDEDefaultClientButton(Client *parent=0, const char *name=0,
                 const unsigned char *bitmap=NULL);
    void setBitmap(const unsigned char *bitmap);
    void setPixmap(const QPixmap &p, const QPixmap &mouseOverPix);
    void reset();
    QSize sizeHint() const;
    int last_button;

protected:
    void mousePressEvent( QMouseEvent* e )
    {
	last_button = e->button();
	QMouseEvent me ( e->type(), e->pos(), e->globalPos(), LeftButton, e->state() );
	QButton::mousePressEvent( &me );
    }
    void mouseReleaseEvent( QMouseEvent* e )
    {
	last_button = e->button();
	QMouseEvent me ( e->type(), e->pos(), e->globalPos(), LeftButton, e->state() );
	QButton::mouseReleaseEvent( &me );
    }
    void enterEvent(QEvent *){isMouseOver=true; repaint(false);}
    void leaveEvent(QEvent *){isMouseOver=false; repaint(false);}
    virtual void drawButton(QPainter *p);
    void drawButtonLabel(QPainter *){;}
    QSize defaultSize;
    QBitmap deco;
    QPixmap pix, moPix;
    Client *client;
    bool isMouseOver;
};

class KDEClient : public Client
{
    Q_OBJECT
public:
    enum Buttons{BtnHelp=0, BtnSticky, BtnMax, BtnIconify, BtnClose, BtnMenu};
    KDEClient( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~KDEClient(){;}
protected:
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
    void showEvent( QShowEvent* );
    void mouseDoubleClickEvent( QMouseEvent * );
    void init();
    void captionChange( const QString& name );
    void stickyChange(bool on);
    void maximizeChange(bool m);
    void doShape();
    void activeChange(bool);

    void calcHiddenButtons();
    void updateActiveBuffer();

    MousePosition mousePosition(const QPoint &) const;

protected slots:
    void slotReset();
    void slotMaximize();
    void menuButtonPressed();
private:
    KDEDefaultClientButton* button[6];
    int lastButtonWidth;
    QSpacerItem* titlebar;
    bool hiddenItems;
    QHBoxLayout *hb;
    KPixmap activeBuffer;
    bool bufferDirty;
    int lastBufferWidth;
    KPixmap lightIcon;
};




#endif
